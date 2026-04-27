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

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "lookup-vptree-8bit.h"
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

struct sixel_lookup_vptree_8bit {
    sixel_allocator_t *allocator;
    unsigned char const *palette;
    int ncolors;
    int depth;
    sixel_lookup_vptree_8bit_node_t *nodes;
    int node_count;
    int root;
    int *safe_dist2;
    int cache_enabled;
    int cached_index;
};

/*
 * VP-tree lookup flow summary:
 *
 *   [cached index] --dist--> (safe?) --yes--> return cached
 *                       |
 *                       no
 *                       v
 *   [VP-tree traversal] --pivot dist--> (safe?) --yes--> return pivot
 *                       |
 *                       v
 *                 branch+bound search
 *
 * The safe distance for each palette entry is computed once as:
 *
 *   safe_dist2[i] = nearest_palette_dist2[i] / 4
 *
 * This allows early confirmation of the nearest palette entry while
 * preserving correctness.
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
    tree->node_count = 0;
    tree->root = -1;
    tree->cached_index = -1;
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
sixel_lookup_vptree_8bit_search(sixel_lookup_vptree_8bit_t *tree,
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
    tree->cached_index = -1;

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
                                   int depth,
                                   int parallel_dither_active)
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
    tree->cache_enabled = (parallel_dither_active == 0);
    tree->cached_index = -1;

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

    return SIXEL_OK;
}

int
sixel_lookup_vptree_8bit_map(sixel_lookup_vptree_8bit_t *tree,
                             unsigned char const *pixel)
{
    int best_index;
    int best_distance;
    int cached_index;
    int distance;

    if (tree == NULL || pixel == NULL || tree->root < 0) {
        return 0;
    }

    best_index = 0;
    best_distance = INT_MAX;
    if (tree->cache_enabled && tree->cached_index >= 0) {
        cached_index = tree->cached_index;
        distance = sixel_lookup_vptree_8bit_pixel_distance(tree,
                                                           pixel,
                                                           cached_index);
        if (distance <= tree->safe_dist2[cached_index]) {
            return cached_index;
        }
        best_distance = distance;
        best_index = cached_index;
    }

    (void)sixel_lookup_vptree_8bit_search(tree,
                                          tree->root,
                                          pixel,
                                          &best_index,
                                          &best_distance);

    if (tree->cache_enabled) {
        tree->cached_index = best_index;
    }

    return best_index;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
