/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
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
#include "lookup-common.h"
#include "lookup-vptree-float32.h"
#include "status.h"

typedef struct sixel_lookup_vptree_float32_node {
    int pivot;
    int left;
    int right;
    float radius;
} sixel_lookup_vptree_float32_node_t;

typedef struct sixel_lookup_vptree_float32_pair {
    int index;
    float distance;
} sixel_lookup_vptree_float32_pair_t;

struct sixel_lookup_vptree_float32 {
    sixel_allocator_t *allocator;
    float const *palette;
    int ncolors;
    int depth;
    float weights[3];
    sixel_lookup_vptree_float32_node_t *nodes;
    int node_count;
    int root;
    float *safe_dist2;
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
    tree->node_count = 0;
    tree->root = -1;
    tree->cached_index = -1;
}

static float
sixel_lookup_vptree_float32_palette_distance(
    sixel_lookup_vptree_float32_t const *tree,
    int left_index,
    int right_index)
{
    int component;
    float diff;
    float distance;
    float const *left;
    float const *right;

    distance = 0.0f;
    left = tree->palette + (size_t)left_index * (size_t)tree->depth;
    right = tree->palette + (size_t)right_index * (size_t)tree->depth;
    for (component = 0; component < 3; ++component) {
        diff = left[component] - right[component];
        distance += diff * diff * tree->weights[component];
    }

    return distance;
}

static float
sixel_lookup_vptree_float32_pixel_distance(
    sixel_lookup_vptree_float32_t const *tree,
    float const *pixel,
    int palette_index)
{
    int component;
    float diff;
    float distance;
    float const *entry;

    distance = 0.0f;
    entry = tree->palette + (size_t)palette_index * (size_t)tree->depth;
    for (component = 0; component < 3; ++component) {
        diff = pixel[component] - entry[component];
        distance += diff * diff * tree->weights[component];
    }

    return distance;
}

static SIXELSTATUS
sixel_lookup_vptree_float32_prepare_safe_distances(
    sixel_lookup_vptree_float32_t *tree)
{
    int index;
    int other;
    float best;
    float distance;

    if (tree->safe_dist2 == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0; index < tree->ncolors; ++index) {
        best = FLT_MAX;
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
        if (best == FLT_MAX) {
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
    float distance;

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
    node->radius = sqrtf(pairs[mid].distance);

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
sixel_lookup_vptree_float32_search(sixel_lookup_vptree_float32_t *tree,
                                   int node_index,
                                   float const *pixel,
                                   int *best_index,
                                   float *best_distance)
{
    sixel_lookup_vptree_float32_node_t const *node;
    int pivot;
    float distance;
    float radius;
    float target;
    float diff;
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
    target = sqrtf(distance);
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
    tree->cached_index = -1;

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
    tree->cache_enabled = (sixel_lookup_parallel_dither_active() == 0);
    tree->cached_index = -1;

    tree->nodes =
        (sixel_lookup_vptree_float32_node_t *)sixel_allocator_malloc(
            tree->allocator,
            (size_t)ncolors * sizeof(sixel_lookup_vptree_float32_node_t));
    if (tree->nodes == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_vptree_float32_configure: node allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    tree->safe_dist2 = (float *)sixel_allocator_malloc(
        tree->allocator,
        (size_t)ncolors * sizeof(float));
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
sixel_lookup_vptree_float32_map(sixel_lookup_vptree_float32_t *tree,
                                float const *pixel)
{
    int best_index;
    float best_distance;
    int cached_index;
    float distance;

    if (tree == NULL || pixel == NULL || tree->root < 0) {
        return 0;
    }

    best_index = 0;
    best_distance = FLT_MAX;
    if (tree->cache_enabled && tree->cached_index >= 0) {
        cached_index = tree->cached_index;
        distance = sixel_lookup_vptree_float32_pixel_distance(tree,
                                                              pixel,
                                                              cached_index);
        if (distance <= tree->safe_dist2[cached_index]) {
            return cached_index;
        }
        best_distance = distance;
        best_index = cached_index;
    }

    (void)sixel_lookup_vptree_float32_search(tree,
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
