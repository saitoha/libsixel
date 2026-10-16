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
 * VP-tree lookup implementation for float32 palettes.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "lookup-vptree-float32.h"
#include "status.h"

typedef struct sixel_lookup_vptree_float32_node {
    int pivot;
    int left;
    int right;
    double radius;
} sixel_lookup_vptree_float32_node_t;

typedef struct sixel_lookup_vptree_float32_pair {
    int index;
    double distance;
} sixel_lookup_vptree_float32_pair_t;

/*
 * One entry of a palette color's neighbor list.  distance is the squared
 * palette distance rounded toward zero to float, so a scan that stops at the
 * first entry above a bound never stops early.
 */
typedef struct sixel_lookup_vptree_float32_neighbor {
    float distance;
    int index;
} sixel_lookup_vptree_float32_neighbor_t;

struct sixel_lookup_vptree_float32 {
    sixel_allocator_t *allocator;
    float const *palette;
    int ncolors;
    int depth;
    float weights[3];
    sixel_lookup_vptree_float32_node_t *nodes;
    int node_count;
    int root;
    double *safe_dist2;
    sixel_lookup_vptree_float32_neighbor_t *neighbors;
    /* Exact double copies of the palette (three components per color) and
     * weights, so query distances skip the per-term float conversions. */
    double *palette_wide;
    double weights_wide[3];
};

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
 * The VP-tree search uses it to confirm a pivot early.  Distances, radii and
 * safe thresholds use double:
 * finite float coordinates can have squared distances outside float range.
 * Keeping only the query distance wide would still corrupt pruning/cache
 * decisions through narrowed construction metadata.
 */
static void
sixel_lookup_vptree_float32_release(sixel_lookup_vptree_float32_t *tree)
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
    if (tree->palette_wide != NULL) {
        sixel_allocator_free(tree->allocator, tree->palette_wide);
        tree->palette_wide = NULL;
    }
    tree->node_count = 0;
    tree->root = -1;
}

static double
sixel_lookup_vptree_float32_palette_distance(
    sixel_lookup_vptree_float32_t const *tree,
    int left_index,
    int right_index)
{
    int component;
    double diff;
    double distance;
    float const *left;
    float const *right;

    distance = 0.0f;
    left = tree->palette + (size_t)left_index * (size_t)tree->depth;
    right = tree->palette + (size_t)right_index * (size_t)tree->depth;
    for (component = 0; component < 3; ++component) {
        diff = (double)left[component] - right[component];
        distance += diff * diff * tree->weights[component];
    }

    return distance;
}

static double
sixel_lookup_vptree_float32_pixel_distance(
    sixel_lookup_vptree_float32_t const *tree,
    float const *pixel,
    int palette_index)
{
    int component;
    double diff;
    double distance;
    double const *entry;

    /* Same arithmetic as converting the float palette and weights per
     * term: the double copies hold exactly those values. */
    distance = 0.0f;
    entry = tree->palette_wide + (size_t)palette_index * 3U;
    for (component = 0; component < 3; ++component) {
        diff = (double)pixel[component] - entry[component];
        distance += diff * diff * tree->weights_wide[component];
    }

    return distance;
}

static SIXELSTATUS
sixel_lookup_vptree_float32_prepare_safe_distances(
    sixel_lookup_vptree_float32_t *tree)
{
    int index;
    int other;
    double best;
    double distance;

    if (tree->safe_dist2 == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0; index < tree->ncolors; ++index) {
        best = DBL_MAX;
        for (other = 0; other < tree->ncolors; ++other) {
            if (other == index) {
                continue;
            }
            distance = sixel_lookup_vptree_float32_palette_distance(tree,
                                                                    index,
                                                                    other);
            if (distance < best) {
                best = distance;
            }
        }
        if (best == DBL_MAX) {
            best = 0.0f;
        }
        tree->safe_dist2[index] = best * 0.25f;
    }

    return SIXEL_OK;
}

static int
sixel_lookup_vptree_float32_pair_compare(void const *left,
                                         void const *right)
{
    sixel_lookup_vptree_float32_pair_t const *a;
    sixel_lookup_vptree_float32_pair_t const *b;

    a = (sixel_lookup_vptree_float32_pair_t const *)left;
    b = (sixel_lookup_vptree_float32_pair_t const *)right;
    if (a->distance < b->distance) {
        return -1;
    }
    if (a->distance > b->distance) {
        return 1;
    }
    return 0;
}

static SIXELSTATUS
sixel_lookup_vptree_float32_prepare_neighbors(
    sixel_lookup_vptree_float32_t *tree)
{
    sixel_lookup_vptree_float32_pair_t *pairs;
    sixel_lookup_vptree_float32_neighbor_t *list;
    int index;
    int other;
    float rounded;

    pairs = (sixel_lookup_vptree_float32_pair_t *)sixel_allocator_malloc(
        tree->allocator,
        (size_t)tree->ncolors * sizeof(sixel_lookup_vptree_float32_pair_t));
    if (pairs == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0; index < tree->ncolors; ++index) {
        for (other = 0; other < tree->ncolors; ++other) {
            pairs[other].index = other;
            pairs[other].distance =
                sixel_lookup_vptree_float32_palette_distance(tree,
                                                             index,
                                                             other);
        }
        qsort(pairs,
              (size_t)tree->ncolors,
              sizeof(sixel_lookup_vptree_float32_pair_t),
              sixel_lookup_vptree_float32_pair_compare);
        list = tree->neighbors + (size_t)index * (size_t)tree->ncolors;
        for (other = 0; other < tree->ncolors; ++other) {
            rounded = (float)pairs[other].distance;
            if ((double)rounded > pairs[other].distance) {
                rounded = nextafterf(rounded, 0.0f);
            }
            list[other].distance = rounded;
            list[other].index = pairs[other].index;
        }
    }

    sixel_allocator_free(tree->allocator, pairs);
    return SIXEL_OK;
}

/*
 * Neighbor-list refinement from start (see the flow summary above).  The
 * scan bound carries a relative slack far above the rounding error of the
 * double distances, so rounding can only lengthen a scan, never skip a color
 * that could win.
 */
static int
sixel_lookup_vptree_float32_refine(sixel_lookup_vptree_float32_t const *tree,
                                   float const *pixel,
                                   int start)
{
    sixel_lookup_vptree_float32_neighbor_t const *list;
    int center;
    int best_index;
    int candidate;
    int position;
    int moved;
    double center_distance;
    double best_distance;
    double distance;
    double limit;

    center = start;
    center_distance = sixel_lookup_vptree_float32_pixel_distance(tree,
                                                                 pixel,
                                                                 center);
    best_index = center;
    best_distance = center_distance;
    for (;;) {
        list = tree->neighbors + (size_t)center * (size_t)tree->ncolors;
        limit = center_distance * 4.0 * (1.0 + 1.0e-9);
        moved = 0;
        for (position = 0; position < tree->ncolors; ++position) {
            if ((double)list[position].distance > limit) {
                break;
            }
            candidate = list[position].index;
            if (candidate == center) {
                continue;
            }
            distance = sixel_lookup_vptree_float32_pixel_distance(tree,
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
sixel_lookup_vptree_float32_build(sixel_lookup_vptree_float32_t *tree,
                                  int *indices,
                                  int count)
{
    sixel_lookup_vptree_float32_node_t *node;
    sixel_lookup_vptree_float32_pair_t *pairs;
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
    double distance;

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
    node->radius = 0.0f;

    if (count == 1) {
        return node_index;
    }

    pair_count = count - 1;
    pairs = (sixel_lookup_vptree_float32_pair_t *)sixel_allocator_malloc(
        tree->allocator,
        (size_t)pair_count * sizeof(sixel_lookup_vptree_float32_pair_t));
    if (pairs == NULL) {
        return -1;
    }

    for (index = 0; index < pair_count; ++index) {
        distance = sixel_lookup_vptree_float32_palette_distance(tree,
                                                                pivot,
                                                                indices[index]);
        pairs[index].index = indices[index];
        pairs[index].distance = distance;
    }

    qsort(pairs,
          (size_t)pair_count,
          sizeof(sixel_lookup_vptree_float32_pair_t),
          sixel_lookup_vptree_float32_pair_compare);

    mid = pair_count / 2;
    node->radius = sqrt(pairs[mid].distance);

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
        node->left = sixel_lookup_vptree_float32_build(tree,
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
        node->right = sixel_lookup_vptree_float32_build(tree,
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
sixel_lookup_vptree_float32_search(sixel_lookup_vptree_float32_t const *tree,
                                   int node_index,
                                   float const *pixel,
                                   int *best_index,
                                   double *best_distance)
{
    sixel_lookup_vptree_float32_node_t const *node;
    int pivot;
    double distance;
    double radius;
    double target;
    double diff;
    int search_done;

    if (node_index < 0) {
        return 0;
    }

    node = &tree->nodes[node_index];
    pivot = node->pivot;
    distance = sixel_lookup_vptree_float32_pixel_distance(tree,
                                                          pixel,
                                                          pivot);
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
    target = sqrt(distance);
    search_done = 0;

    if (target < radius) {
        if (node->left >= 0) {
            search_done = sixel_lookup_vptree_float32_search(tree,
                                                             node->left,
                                                             pixel,
                                                             best_index,
                                                             best_distance);
            if (search_done != 0) {
                return 1;
            }
        }
        diff = radius - target;
        if (diff * diff <= *best_distance) {
            if (node->right >= 0) {
                return sixel_lookup_vptree_float32_search(tree,
                                                          node->right,
                                                          pixel,
                                                          best_index,
                                                          best_distance);
            }
        }
    } else {
        if (node->right >= 0) {
            search_done = sixel_lookup_vptree_float32_search(tree,
                                                             node->right,
                                                             pixel,
                                                             best_index,
                                                             best_distance);
            if (search_done != 0) {
                return 1;
            }
        }
        diff = target - radius;
        if (diff * diff <= *best_distance) {
            if (node->left >= 0) {
                return sixel_lookup_vptree_float32_search(tree,
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
sixel_lookup_vptree_float32_create(sixel_allocator_t *allocator,
                                   sixel_lookup_vptree_float32_t **tree_out)
{
    sixel_lookup_vptree_float32_t *tree;

    if (allocator == NULL || tree_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    tree = (sixel_lookup_vptree_float32_t *)sixel_allocator_malloc(
        allocator,
        sizeof(sixel_lookup_vptree_float32_t));
    if (tree == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_create: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(tree, 0, sizeof(sixel_lookup_vptree_float32_t));
    tree->allocator = allocator;
    tree->root = -1;

    *tree_out = tree;
    return SIXEL_OK;
}

void
sixel_lookup_vptree_float32_unref(sixel_lookup_vptree_float32_t *tree)
{
    if (tree == NULL) {
        return;
    }

    sixel_lookup_vptree_float32_release(tree);
    sixel_allocator_free(tree->allocator, tree);
}

SIXELSTATUS
sixel_lookup_vptree_float32_configure(sixel_lookup_vptree_float32_t *tree,
                                      float const *palette,
                                      int ncolors,
                                      int depth,
                                      float const *weights)
{
    SIXELSTATUS status;
    int *indices;
    int index;

    if (tree == NULL || palette == NULL || weights == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (ncolors <= 0 || depth <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_vptree_float32_release(tree);
    tree->palette = palette;
    tree->ncolors = ncolors;
    tree->depth = depth;
    tree->weights[0] = weights[0];
    tree->weights[1] = weights[1];
    tree->weights[2] = weights[2];
    tree->weights_wide[0] = weights[0];
    tree->weights_wide[1] = weights[1];
    tree->weights_wide[2] = weights[2];

    tree->palette_wide = (double *)sixel_allocator_malloc(
        tree->allocator, (size_t)ncolors * 3U * sizeof(double));
    if (tree->palette_wide == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_configure: palette allocation "
            "failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0; index < ncolors * 3; ++index) {
        tree->palette_wide[index] =
            palette[(size_t)(index / 3) * (size_t)depth
                    + (size_t)(index % 3)];
    }

    tree->nodes =
        (sixel_lookup_vptree_float32_node_t *)sixel_allocator_malloc(
            tree->allocator,
            (size_t)ncolors * sizeof(sixel_lookup_vptree_float32_node_t));
    if (tree->nodes == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_configure: node allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    tree->safe_dist2 = (double *)sixel_allocator_malloc(
        tree->allocator,
        (size_t)ncolors * sizeof(double));
    if (tree->safe_dist2 == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_configure: safe distance allocation "
            "failed.");
        sixel_lookup_vptree_float32_release(tree);
        return SIXEL_BAD_ALLOCATION;
    }

    status = sixel_lookup_vptree_float32_prepare_safe_distances(tree);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_vptree_float32_release(tree);
        return status;
    }

    indices = (int *)sixel_allocator_malloc(tree->allocator,
                                            (size_t)ncolors * sizeof(int));
    if (indices == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_configure: index allocation failed.");
        sixel_lookup_vptree_float32_release(tree);
        return SIXEL_BAD_ALLOCATION;
    }

    tree->neighbors = (sixel_lookup_vptree_float32_neighbor_t *)
        sixel_allocator_malloc(
            tree->allocator,
            (size_t)ncolors * (size_t)ncolors
                * sizeof(sixel_lookup_vptree_float32_neighbor_t));
    if (tree->neighbors == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_configure: neighbor allocation "
            "failed.");
        sixel_allocator_free(tree->allocator, indices);
        sixel_lookup_vptree_float32_release(tree);
        return SIXEL_BAD_ALLOCATION;
    }
    status = sixel_lookup_vptree_float32_prepare_neighbors(tree);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_configure: neighbor allocation "
            "failed.");
        sixel_allocator_free(tree->allocator, indices);
        sixel_lookup_vptree_float32_release(tree);
        return status;
    }

    for (index = 0; index < ncolors; ++index) {
        indices[index] = index;
    }

    tree->root = sixel_lookup_vptree_float32_build(tree, indices, ncolors);
    sixel_allocator_free(tree->allocator, indices);
    if (tree->root < 0) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_configure: tree build failed.");
        sixel_lookup_vptree_float32_release(tree);
        return SIXEL_BAD_ALLOCATION;
    }

    return SIXEL_OK;
}

int
sixel_lookup_vptree_float32_map(sixel_lookup_vptree_float32_t const *tree,
                                float const *pixel,
                                int hint)
{
    int best_index;
    double best_distance;

    if (tree == NULL || pixel == NULL || tree->root < 0) {
        return 0;
    }

    best_index = hint;
    if (best_index < 0 || best_index >= tree->ncolors) {
        best_index = 0;
        best_distance = DBL_MAX;
        (void)sixel_lookup_vptree_float32_search(tree,
                                           tree->root,
                                           pixel,
                                           &best_index,
                                           &best_distance);
    }

    return sixel_lookup_vptree_float32_refine(tree, pixel, best_index);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
