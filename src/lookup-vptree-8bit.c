/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * VP-tree lookup implementation for 8-bit palettes.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
/* CPU capability alone does not imply compiler intrinsics are available.
 * Keep the includes and every SIMD branch behind the same header probes. */
#if HAVE_NEON && HAVE_ARM_NEON_H
#include <arm_neon.h>
#elif HAVE_SSE2 && HAVE_EMMINTRIN_H
#include <emmintrin.h>
#endif

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "lookup-vptree-8bit.h"
#include "sixel_atomic.h"
#include "status.h"

typedef struct sixel_lookup_vptree_8bit_node {
    int pivot;
    int left;
    int right;
    double radius;
} sixel_lookup_vptree_8bit_node_t;

typedef struct sixel_lookup_vptree_8bit_pair {
    int index;
    int distance;
} sixel_lookup_vptree_8bit_pair_t;

/* One entry of a palette color's neighbor list (exact squared distance). */
typedef struct sixel_lookup_vptree_8bit_neighbor {
    int distance;
    int index;
} sixel_lookup_vptree_8bit_neighbor_t;

struct sixel_lookup_vptree_8bit {
    sixel_allocator_t *allocator;
    unsigned char const *palette;
    int ncolors;
    int depth;
    sixel_lookup_vptree_8bit_node_t *nodes;
    int node_count;
    int root;
    int *safe_dist2;
    sixel_lookup_vptree_8bit_neighbor_t *neighbors;
    sixel_atomic_i32_t *memo;
};

/*
 * Exact-result memo for RGB queries.  Dithered queries repeat byte values
 * often, so a direct-mapped table of (24-bit key, 8-bit index) words returns
 * most results without a search.  Each slot is one relaxed atomic word: a
 * reader sees either an old complete entry or a new complete entry, and every
 * entry holds the query's nearest index, so concurrent callers can share it
 * and it never changes a result.  The all-ones word marks an empty slot; the
 * one entry with that pattern (white mapped to index 255) is not memoized.
 */
#define SIXEL_LOOKUP_VPTREE_8BIT_MEMO_BITS 18
#define SIXEL_LOOKUP_VPTREE_8BIT_MEMO_EMPTY (-1)

/*
 * VP-tree lookup flow summary:
 *
 *   [caller hint] ------+
 *                       +--> start --> [neighbor-list refinement] --> index
 *   [VP-tree search] ---+    (no usable hint)
 *
 * Successive dither queries are close to each other, so the index the caller
 * mapped for the previous pixel is usually the answer or next to it.
 * Refinement (Orchard's method) scans the start color's neighbors in order
 * of palette distance.  Any color at
 * least as close to the query q as the current center c lies within twice
 * |q - c| of c (triangle inequality), so the scan stops at the first
 * neighbor beyond 4 * |q - c|^2 and moves to a strictly closer color when it
 * finds one.  The result is the nearest color, and among equally near colors
 * the lowest index, whatever the start: the hint therefore changes speed
 * but never the answer, which keeps serial and parallel dithering
 * identical.  The VP-tree supplies the start when the caller has no hint.
 * The tree keeps no scan history, so concurrent queries are safe.
 *
 * The safe distance for each palette entry is computed once as:
 *
 *   safe_dist2[i] = nearest_palette_dist2[i] / 4
 *
 * The VP-tree search uses it to confirm a pivot early.
 */
static void
sixel_lookup_vptree_8bit_release(sixel_lookup_vptree_8bit_t *tree)
{
    if (tree->nodes != NULL) {
        sixel_allocator_free(tree->allocator, tree->nodes);
        tree->nodes = NULL;
    }
    if (tree->safe_dist2 != NULL) {
        sixel_allocator_free(tree->allocator, tree->safe_dist2);
        tree->safe_dist2 = NULL;
    }
    if (tree->neighbors != NULL) {
        sixel_allocator_free(tree->allocator, tree->neighbors);
        tree->neighbors = NULL;
    }
    if (tree->memo != NULL) {
        sixel_allocator_free(tree->allocator, tree->memo);
        tree->memo = NULL;
    }
    tree->node_count = 0;
    tree->root = -1;
}

static int
sixel_lookup_vptree_8bit_palette_distance(
    sixel_lookup_vptree_8bit_t const *tree,
    int left_index,
    int right_index)
{
    unsigned char const *left;
    unsigned char const *right;
    int depth;
    int left0;
    int left1;
    int left2;
    int right0;
    int right1;
    int right2;
    int diff;
    int distance;

    depth = tree->depth;
    left = tree->palette + (size_t)left_index * (size_t)depth;
    right = tree->palette + (size_t)right_index * (size_t)depth;
    left0 = (depth > 0) ? (int)left[0] : 0;
    left1 = (depth > 1) ? (int)left[1] : 0;
    left2 = (depth > 2) ? (int)left[2] : 0;
    right0 = (depth > 0) ? (int)right[0] : 0;
    right1 = (depth > 1) ? (int)right[1] : 0;
    right2 = (depth > 2) ? (int)right[2] : 0;

    diff = left0 - right0;
    distance = diff * diff;
    diff = left1 - right1;
    distance += diff * diff;
    diff = left2 - right2;
    distance += diff * diff;

    return distance;
}

static int
sixel_lookup_vptree_8bit_pixel_distance(
    sixel_lookup_vptree_8bit_t const *tree,
    unsigned char const *pixel,
    int palette_index)
{
    unsigned char const *entry;
    int depth;
    int pixel0;
    int pixel1;
    int pixel2;
    int entry0;
    int entry1;
    int entry2;
    int diff;
    int distance;

    depth = tree->depth;
    entry = tree->palette + (size_t)palette_index * (size_t)depth;
    pixel0 = (depth > 0) ? (int)pixel[0] : 0;
    pixel1 = (depth > 1) ? (int)pixel[1] : 0;
    pixel2 = (depth > 2) ? (int)pixel[2] : 0;
    entry0 = (depth > 0) ? (int)entry[0] : 0;
    entry1 = (depth > 1) ? (int)entry[1] : 0;
    entry2 = (depth > 2) ? (int)entry[2] : 0;

    diff = pixel0 - entry0;
    distance = diff * diff;
    diff = pixel1 - entry1;
    distance += diff * diff;
    diff = pixel2 - entry2;
    distance += diff * diff;

    return distance;
}

static SIXELSTATUS
sixel_lookup_vptree_8bit_prepare_safe_distances(
    sixel_lookup_vptree_8bit_t *tree)
{
    int index;
    int other;
    int best;
    int distance;

    if (tree->safe_dist2 == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0; index < tree->ncolors; ++index) {
        best = INT_MAX;
        for (other = 0; other < tree->ncolors; ++other) {
            if (other == index) {
                continue;
            }
            distance = sixel_lookup_vptree_8bit_palette_distance(tree,
                                                                 index,
                                                                 other);
            if (distance < best) {
                best = distance;
            }
        }
        if (best == INT_MAX) {
            best = 0;
        }
        tree->safe_dist2[index] = best / 4;
    }

    return SIXEL_OK;
}

static int
sixel_lookup_vptree_8bit_pair_compare(void const *left,
                                      void const *right)
{
    sixel_lookup_vptree_8bit_pair_t const *a;
    sixel_lookup_vptree_8bit_pair_t const *b;

    a = (sixel_lookup_vptree_8bit_pair_t const *)left;
    b = (sixel_lookup_vptree_8bit_pair_t const *)right;
    if (a->distance < b->distance) {
        return -1;
    }
    if (a->distance > b->distance) {
        return 1;
    }
    return 0;
}

static SIXELSTATUS
sixel_lookup_vptree_8bit_prepare_neighbors(sixel_lookup_vptree_8bit_t *tree)
{
    sixel_lookup_vptree_8bit_pair_t *pairs;
    sixel_lookup_vptree_8bit_neighbor_t *list;
    int index;
    int other;

    pairs = (sixel_lookup_vptree_8bit_pair_t *)sixel_allocator_malloc(
        tree->allocator,
        (size_t)tree->ncolors * sizeof(sixel_lookup_vptree_8bit_pair_t));
    if (pairs == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0; index < tree->ncolors; ++index) {
        for (other = 0; other < tree->ncolors; ++other) {
            pairs[other].index = other;
            pairs[other].distance =
                sixel_lookup_vptree_8bit_palette_distance(tree, index, other);
        }
        qsort(pairs,
              (size_t)tree->ncolors,
              sizeof(sixel_lookup_vptree_8bit_pair_t),
              sixel_lookup_vptree_8bit_pair_compare);
        list = tree->neighbors + (size_t)index * (size_t)tree->ncolors;
        for (other = 0; other < tree->ncolors; ++other) {
            list[other].distance = pairs[other].distance;
            list[other].index = pairs[other].index;
        }
    }

    sixel_allocator_free(tree->allocator, pairs);
    return SIXEL_OK;
}

/*
 * Neighbor-list refinement from start (see the flow summary above).  Integer
 * distances are exact, so the scan bound needs no slack.
 */
static int
sixel_lookup_vptree_8bit_refine(sixel_lookup_vptree_8bit_t const *tree,
                                unsigned char const *pixel,
                                int start)
{
    sixel_lookup_vptree_8bit_neighbor_t const *list;
    int center;
    int best_index;
    int candidate;
    int position;
    int moved;
    int center_distance;
    int best_distance;
    int distance;
    int limit;

    center = start;
    center_distance = sixel_lookup_vptree_8bit_pixel_distance(tree,
                                                              pixel,
                                                              center);
    best_index = center;
    best_distance = center_distance;
    for (;;) {
        list = tree->neighbors + (size_t)center * (size_t)tree->ncolors;
        /* 4 * 3 * 255^2 stays far below INT_MAX. */
        limit = center_distance * 4;
        moved = 0;
        for (position = 0; position < tree->ncolors; ++position) {
            if (list[position].distance > limit) {
                break;
            }
            candidate = list[position].index;
            if (candidate == center) {
                continue;
            }
            distance = sixel_lookup_vptree_8bit_pixel_distance(tree,
                                                               pixel,
                                                               candidate);
            if (distance < best_distance
                    || (distance == best_distance
                        && candidate < best_index)) {
                best_distance = distance;
                best_index = candidate;
                if (distance < center_distance) {
                    moved = 1;
                    break;
                }
            }
        }
        if (moved == 0) {
            return best_index;
        }
        center = best_index;
        center_distance = best_distance;
    }
}

static int
sixel_lookup_vptree_8bit_build(sixel_lookup_vptree_8bit_t *tree,
                               int *indices,
                               int count)
{
    sixel_lookup_vptree_8bit_node_t *node;
    sixel_lookup_vptree_8bit_pair_t *pairs;
    int node_index;
    int pivot;
    int pair_count;
    int mid;
    int left_count;
    int right_count;
    int *left_indices;
    int *right_indices;
    int index;
    int offset;
    int distance;

    if (count <= 0) {
        return -1;
    }
    node_index = tree->node_count;
    if (node_index >= tree->ncolors) {
        return -1;
    }

    tree->node_count++;
    node = &tree->nodes[node_index];
    pivot = indices[count - 1];
    node->pivot = pivot;
    node->left = -1;
    node->right = -1;
    node->radius = 0.0;

    if (count == 1) {
        return node_index;
    }

    pair_count = count - 1;
    pairs = (sixel_lookup_vptree_8bit_pair_t *)sixel_allocator_malloc(
        tree->allocator,
        (size_t)pair_count * sizeof(sixel_lookup_vptree_8bit_pair_t));
    if (pairs == NULL) {
        return -1;
    }

    for (index = 0; index < pair_count; ++index) {
        distance = sixel_lookup_vptree_8bit_palette_distance(tree,
                                                             pivot,
                                                             indices[index]);
        pairs[index].index = indices[index];
        pairs[index].distance = distance;
    }

    qsort(pairs,
          (size_t)pair_count,
          sizeof(sixel_lookup_vptree_8bit_pair_t),
          sixel_lookup_vptree_8bit_pair_compare);

    mid = pair_count / 2;
    node->radius = sqrt((double)pairs[mid].distance);

    left_count = mid;
    right_count = pair_count - mid;
    left_indices = NULL;
    right_indices = NULL;

    if (left_count > 0) {
        left_indices = (int *)sixel_allocator_malloc(
            tree->allocator,
            (size_t)left_count * sizeof(int));
        if (left_indices == NULL) {
            sixel_allocator_free(tree->allocator, pairs);
            return -1;
        }
        for (index = 0; index < left_count; ++index) {
            left_indices[index] = pairs[index].index;
        }
        node->left = sixel_lookup_vptree_8bit_build(tree,
                                                    left_indices,
                                                    left_count);
        sixel_allocator_free(tree->allocator, left_indices);
        if (node->left < 0) {
            sixel_allocator_free(tree->allocator, pairs);
            return -1;
        }
    }

    if (right_count > 0) {
        right_indices = (int *)sixel_allocator_malloc(
            tree->allocator,
            (size_t)right_count * sizeof(int));
        if (right_indices == NULL) {
            sixel_allocator_free(tree->allocator, pairs);
            return -1;
        }
        offset = 0;
        for (index = mid; index < pair_count; ++index) {
            right_indices[offset] = pairs[index].index;
            offset++;
        }
        node->right = sixel_lookup_vptree_8bit_build(tree,
                                                     right_indices,
                                                     right_count);
        sixel_allocator_free(tree->allocator, right_indices);
        if (node->right < 0) {
            sixel_allocator_free(tree->allocator, pairs);
            return -1;
        }
    }

    sixel_allocator_free(tree->allocator, pairs);
    return node_index;
}

static int
sixel_lookup_vptree_8bit_search(sixel_lookup_vptree_8bit_t const *tree,
                                int node_index,
                                unsigned char const *pixel,
                                int *best_index,
                                int *best_distance)
{
    sixel_lookup_vptree_8bit_node_t const *node;
    int pivot;
    int distance;
    double radius;
    double target;
    double diff;
    int search_done;

    if (node_index < 0) {
        return 0;
    }

    node = &tree->nodes[node_index];
    pivot = node->pivot;
    distance = sixel_lookup_vptree_8bit_pixel_distance(tree, pixel, pivot);
    if (distance <= tree->safe_dist2[pivot]) {
        *best_index = pivot;
        *best_distance = distance;
        return 1;
    }
    if (distance < *best_distance) {
        *best_distance = distance;
        *best_index = pivot;
    }

    radius = node->radius;
    target = sqrt((double)distance);
    search_done = 0;

    if (target < radius) {
        if (node->left >= 0) {
            search_done = sixel_lookup_vptree_8bit_search(tree,
                                                          node->left,
                                                          pixel,
                                                          best_index,
                                                          best_distance);
            if (search_done != 0) {
                return 1;
            }
        }
        diff = radius - target;
        if (diff * diff <= (double)(*best_distance)) {
            if (node->right >= 0) {
                return sixel_lookup_vptree_8bit_search(tree,
                                                       node->right,
                                                       pixel,
                                                       best_index,
                                                       best_distance);
            }
        }
    } else {
        if (node->right >= 0) {
            search_done = sixel_lookup_vptree_8bit_search(tree,
                                                          node->right,
                                                          pixel,
                                                          best_index,
                                                          best_distance);
            if (search_done != 0) {
                return 1;
            }
        }
        diff = target - radius;
        if (diff * diff <= (double)(*best_distance)) {
            if (node->left >= 0) {
                return sixel_lookup_vptree_8bit_search(tree,
                                                       node->left,
                                                       pixel,
                                                       best_index,
                                                       best_distance);
            }
        }
    }

    return 0;
}

SIXELSTATUS
sixel_lookup_vptree_8bit_create(sixel_allocator_t *allocator,
                                sixel_lookup_vptree_8bit_t **tree_out)
{
    sixel_lookup_vptree_8bit_t *tree;

    if (allocator == NULL || tree_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    tree = (sixel_lookup_vptree_8bit_t *)sixel_allocator_malloc(
        allocator,
        sizeof(sixel_lookup_vptree_8bit_t));
    if (tree == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_8bit_create: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(tree, 0, sizeof(sixel_lookup_vptree_8bit_t));
    tree->allocator = allocator;
    tree->root = -1;

    *tree_out = tree;
    return SIXEL_OK;
}

void
sixel_lookup_vptree_8bit_unref(sixel_lookup_vptree_8bit_t *tree)
{
    if (tree == NULL) {
        return;
    }

    sixel_lookup_vptree_8bit_release(tree);
    sixel_allocator_free(tree->allocator, tree);
}

SIXELSTATUS
sixel_lookup_vptree_8bit_configure(sixel_lookup_vptree_8bit_t *tree,
                                   unsigned char const *palette,
                                   int ncolors,
                                   int depth)
{
    SIXELSTATUS status;
    int *indices;
    int index;

    if (tree == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (ncolors <= 0 || depth <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_vptree_8bit_release(tree);
    tree->palette = palette;
    tree->ncolors = ncolors;
    tree->depth = depth;

    tree->nodes = (sixel_lookup_vptree_8bit_node_t *)sixel_allocator_malloc(
        tree->allocator,
        (size_t)ncolors * sizeof(sixel_lookup_vptree_8bit_node_t));
    if (tree->nodes == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_8bit_configure: node allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    tree->safe_dist2 = (int *)sixel_allocator_malloc(
        tree->allocator,
        (size_t)ncolors * sizeof(int));
    if (tree->safe_dist2 == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_8bit_configure: safe distance allocation "
            "failed.");
        sixel_lookup_vptree_8bit_release(tree);
        return SIXEL_BAD_ALLOCATION;
    }

    status = sixel_lookup_vptree_8bit_prepare_safe_distances(tree);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_vptree_8bit_release(tree);
        return status;
    }

    indices = (int *)sixel_allocator_malloc(tree->allocator,
                                            (size_t)ncolors * sizeof(int));
    if (indices == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_8bit_configure: index allocation failed.");
        sixel_lookup_vptree_8bit_release(tree);
        return SIXEL_BAD_ALLOCATION;
    }

    tree->neighbors = (sixel_lookup_vptree_8bit_neighbor_t *)
        sixel_allocator_malloc(
            tree->allocator,
            (size_t)ncolors * (size_t)ncolors
                * sizeof(sixel_lookup_vptree_8bit_neighbor_t));
    if (tree->neighbors == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_8bit_configure: neighbor allocation failed.");
        sixel_allocator_free(tree->allocator, indices);
        sixel_lookup_vptree_8bit_release(tree);
        return SIXEL_BAD_ALLOCATION;
    }
    status = sixel_lookup_vptree_8bit_prepare_neighbors(tree);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_8bit_configure: neighbor allocation failed.");
        sixel_allocator_free(tree->allocator, indices);
        sixel_lookup_vptree_8bit_release(tree);
        return status;
    }

    for (index = 0; index < ncolors; ++index) {
        indices[index] = index;
    }

    tree->root = sixel_lookup_vptree_8bit_build(tree, indices, ncolors);
    sixel_allocator_free(tree->allocator, indices);
    if (tree->root < 0) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_8bit_configure: tree build failed.");
        sixel_lookup_vptree_8bit_release(tree);
        return SIXEL_BAD_ALLOCATION;
    }

    /* The memo packs a byte index, so it serves RGB palettes of up to 256
     * colors; other shapes search every time. */
    if (depth == 3 && ncolors <= 256) {
        tree->memo = (sixel_atomic_i32_t *)sixel_allocator_malloc(
            tree->allocator,
            ((size_t)1 << SIXEL_LOOKUP_VPTREE_8BIT_MEMO_BITS)
                * sizeof(sixel_atomic_i32_t));
        if (tree->memo == NULL) {
            sixel_helper_set_additional_message(
                "sixel_lookup_vptree_8bit_configure: memo allocation "
                "failed.");
            sixel_lookup_vptree_8bit_release(tree);
            return SIXEL_BAD_ALLOCATION;
        }
        for (index = 0;
                index < (1 << SIXEL_LOOKUP_VPTREE_8BIT_MEMO_BITS);
                ++index) {
            sixel_atomic_store_relaxed_i32(&tree->memo[index],
                                           SIXEL_LOOKUP_VPTREE_8BIT_MEMO_EMPTY);
        }
    }

    return SIXEL_OK;
}

int
sixel_lookup_vptree_8bit_map(sixel_lookup_vptree_8bit_t const *tree,
                             unsigned char const *pixel,
                             int hint)
{
    int best_index;
    int best_distance;
    unsigned int key;
    unsigned int slot;
    unsigned int entry;

    if (tree == NULL || pixel == NULL || tree->root < 0) {
        return 0;
    }

    key = 0U;
    slot = 0U;
    if (tree->memo != NULL) {
        key = ((unsigned int)pixel[0] << 16)
            | ((unsigned int)pixel[1] << 8)
            | (unsigned int)pixel[2];
        slot = (key * 2654435761U) >> (32 - SIXEL_LOOKUP_VPTREE_8BIT_MEMO_BITS);
        entry = (unsigned int)sixel_atomic_load_relaxed_i32(&tree->memo[slot]);
        if ((entry >> 8) == key
                && entry != (unsigned int)SIXEL_LOOKUP_VPTREE_8BIT_MEMO_EMPTY) {
            return (int)(entry & 0xffU);
        }
    }

    best_index = hint;
    if (best_index < 0 || best_index >= tree->ncolors) {
        best_index = 0;
        best_distance = INT_MAX;
        (void)sixel_lookup_vptree_8bit_search(tree,
                                        tree->root,
                                        pixel,
                                        &best_index,
                                        &best_distance);
    }

    best_index = sixel_lookup_vptree_8bit_refine(tree, pixel, best_index);
    if (tree->memo != NULL) {
        entry = (key << 8) | (unsigned int)best_index;
        if (entry != (unsigned int)SIXEL_LOOKUP_VPTREE_8BIT_MEMO_EMPTY) {
            sixel_atomic_store_relaxed_i32(&tree->memo[slot], (int)entry);
        }
    }

    return best_index;
}

#if (HAVE_NEON && HAVE_ARM_NEON_H) || (HAVE_SSE2 && HAVE_EMMINTRIN_H)
/* Eight independent exhaustive searches amortize SIMD work over cold memo
 * entries. Strict comparison in palette order preserves the lowest-index
 * tie rule, including duplicate colors. All squared byte distances fit in
 * signed 32 bits. Warm entries never pay for this scan. */
static void
sixel_lookup_vptree_8bit_map_cold8(
    sixel_lookup_vptree_8bit_t const *tree,
    unsigned int const *keys, int *indexes)
{
    int16_t red[8];
    int16_t green[8];
    int16_t blue[8];
    int i;
    int j;
    unsigned char const *p;
#if HAVE_NEON && HAVE_ARM_NEON_H
    int16x8_t r, g, b, dr, dg, db;
    int32x4_t lo, hi, best_lo, best_hi, idx_lo, idx_hi, index;
    uint32x4_t mask;
#else
    __m128i r, g, b, dr, dg, db, zero;
    __m128i lo, hi, best_lo, best_hi, idx_lo, idx_hi, index, mask, v;
#endif

    for (j = 0; j < 8; ++j) {
        red[j] = (int16_t)(keys[j] >> 16);
        green[j] = (int16_t)((keys[j] >> 8) & 255U);
        blue[j] = (int16_t)(keys[j] & 255U);
    }
#if HAVE_NEON && HAVE_ARM_NEON_H
    r = vld1q_s16(red);
    g = vld1q_s16(green);
    b = vld1q_s16(blue);
    best_lo = best_hi = vdupq_n_s32(INT_MAX);
    idx_lo = idx_hi = vdupq_n_s32(0);
#else
    r = _mm_loadu_si128((__m128i const *)(void const *)red);
    g = _mm_loadu_si128((__m128i const *)(void const *)green);
    b = _mm_loadu_si128((__m128i const *)(void const *)blue);
    best_lo = best_hi = _mm_set1_epi32(INT_MAX);
    zero = idx_lo = idx_hi = _mm_setzero_si128();
#endif
    for (i = 0; i < tree->ncolors; ++i) {
        p = tree->palette + i * 3;
#if HAVE_NEON && HAVE_ARM_NEON_H
        dr = vsubq_s16(r, vdupq_n_s16(p[0]));
        dg = vsubq_s16(g, vdupq_n_s16(p[1]));
        db = vsubq_s16(b, vdupq_n_s16(p[2]));
        lo = vmull_s16(vget_low_s16(dr), vget_low_s16(dr));
        hi = vmull_s16(vget_high_s16(dr), vget_high_s16(dr));
        lo = vmlal_s16(lo, vget_low_s16(dg), vget_low_s16(dg));
        hi = vmlal_s16(hi, vget_high_s16(dg), vget_high_s16(dg));
        lo = vmlal_s16(lo, vget_low_s16(db), vget_low_s16(db));
        hi = vmlal_s16(hi, vget_high_s16(db), vget_high_s16(db));
        index = vdupq_n_s32(i);
        mask = vcltq_s32(lo, best_lo);
        idx_lo = vbslq_s32(mask, index, idx_lo);
        best_lo = vminq_s32(lo, best_lo);
        mask = vcltq_s32(hi, best_hi);
        idx_hi = vbslq_s32(mask, index, idx_hi);
        best_hi = vminq_s32(hi, best_hi);
#else
        dr = _mm_sub_epi16(r, _mm_set1_epi16(p[0]));
        dg = _mm_sub_epi16(g, _mm_set1_epi16(p[1]));
        db = _mm_sub_epi16(b, _mm_set1_epi16(p[2]));
        v = _mm_unpacklo_epi16(dr, zero);
        lo = _mm_madd_epi16(v, v);
        v = _mm_unpackhi_epi16(dr, zero);
        hi = _mm_madd_epi16(v, v);
        v = _mm_unpacklo_epi16(dg, zero);
        lo = _mm_add_epi32(lo, _mm_madd_epi16(v, v));
        v = _mm_unpackhi_epi16(dg, zero);
        hi = _mm_add_epi32(hi, _mm_madd_epi16(v, v));
        v = _mm_unpacklo_epi16(db, zero);
        lo = _mm_add_epi32(lo, _mm_madd_epi16(v, v));
        v = _mm_unpackhi_epi16(db, zero);
        hi = _mm_add_epi32(hi, _mm_madd_epi16(v, v));
        index = _mm_set1_epi32(i);
        mask = _mm_cmplt_epi32(lo, best_lo);
        idx_lo = _mm_or_si128(_mm_and_si128(mask, index),
                              _mm_andnot_si128(mask, idx_lo));
        best_lo = _mm_or_si128(_mm_and_si128(mask, lo),
                               _mm_andnot_si128(mask, best_lo));
        mask = _mm_cmplt_epi32(hi, best_hi);
        idx_hi = _mm_or_si128(_mm_and_si128(mask, index),
                              _mm_andnot_si128(mask, idx_hi));
        best_hi = _mm_or_si128(_mm_and_si128(mask, hi),
                               _mm_andnot_si128(mask, best_hi));
#endif
    }
#if HAVE_NEON && HAVE_ARM_NEON_H
    vst1q_s32(indexes, idx_lo);
    vst1q_s32(indexes + 4, idx_hi);
#else
    _mm_storeu_si128((__m128i *)(void *)indexes, idx_lo);
    _mm_storeu_si128((__m128i *)(void *)(indexes + 4), idx_hi);
#endif
}
#endif

/* Map an ordered row with caller-local continuity. The shared exact-key memo
 * remains the sole persistent cache; no scan state escapes this call. */
void
sixel_lookup_vptree_8bit_map_pixels(
    sixel_lookup_vptree_8bit_t const *tree,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride)
{
    size_t i;
    unsigned int key;
    unsigned int previous;
    int hint;
#if (HAVE_NEON && HAVE_ARM_NEON_H) || (HAVE_SSE2 && HAVE_EMMINTRIN_H)
    unsigned int keys[8];
    unsigned int slots[8];
    int indexes[8];
    sixel_index_t *outputs[8];
    unsigned int slot;
    unsigned int entry;
    int pending;
    int j;
    unsigned char pixel[3];
#endif

    previous = 0x1000000U;
    hint = -1;
#if (HAVE_NEON && HAVE_ARM_NEON_H) || (HAVE_SSE2 && HAVE_EMMINTRIN_H)
    pending = 0;
#endif
    for (i = 0; i < count; ++i) {
        key = ((unsigned int)pixels[0] << 16)
            | ((unsigned int)pixels[1] << 8) | pixels[2];
#if (HAVE_NEON && HAVE_ARM_NEON_H) || (HAVE_SSE2 && HAVE_EMMINTRIN_H)
        if (tree->memo != NULL && key != previous) {
            slot = (key * 2654435761U)
                >> (32 - SIXEL_LOOKUP_VPTREE_8BIT_MEMO_BITS);
            entry = (unsigned int)sixel_atomic_load_relaxed_i32(
                &tree->memo[slot]);
            if ((entry >> 8) == key && entry != (unsigned int)-1) {
                hint = (int)(entry & 255U);
                previous = key;
                *result = (sixel_index_t)hint;
            } else {
                /* Delay only exact independent misses. Repeats of a pending
                 * key may occupy another lane, but never read stale output. */
                keys[pending] = key;
                slots[pending] = slot;
                outputs[pending++] = result;
                if (pending == 8) {
                    sixel_lookup_vptree_8bit_map_cold8(tree, keys, indexes);
                    for (j = 0; j < 8; ++j) {
                        *outputs[j] = (sixel_index_t)indexes[j];
                        entry = (keys[j] << 8) | (unsigned int)indexes[j];
                        if (entry != (unsigned int)-1) {
                            sixel_atomic_store_relaxed_i32(
                                &tree->memo[slots[j]], (int)entry);
                        }
                    }
                    pending = 0;
                    hint = indexes[7];
                    previous = key;
                }
            }
        } else
#endif
        {
            if (key != previous) {
                hint = sixel_lookup_vptree_8bit_map(tree, pixels, hint);
                previous = key;
            }
            *result = (sixel_index_t)hint;
        }
        if (i + 1 < count) {
            pixels += pixel_stride;
            result += result_stride;
        }
    }
#if (HAVE_NEON && HAVE_ARM_NEON_H) || (HAVE_SSE2 && HAVE_EMMINTRIN_H)
    /* A short tail uses the ordinary exact search without reading padding. */
    for (j = 0; j < pending; ++j) {
        pixel[0] = (unsigned char)(keys[j] >> 16);
        pixel[1] = (unsigned char)(keys[j] >> 8);
        pixel[2] = (unsigned char)keys[j];
        hint = sixel_lookup_vptree_8bit_map(tree, pixel, hint);
        *outputs[j] = (sixel_index_t)hint;
    }
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
