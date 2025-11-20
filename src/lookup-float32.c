/*
 * SPDX-License-Identifier: MIT
 */

/*
 * Float32-aware lookup backend.  The search avoids byte quantization so
 * floating-point inputs keep their precision when resolving palette entries.
 * The CERT LUT path uses a lightweight kd-tree while other policies fall back
 * to a linear scan over the palette.
 */

#include "config.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "lookup-float32.h"
#include "pixelformat.h"
#include "status.h"

struct sixel_lookup_float32_node {
    int index;
    int left;
    int right;
    int axis;
};

static int
sixel_lookup_float32_policy_normalize(int policy)
{
    int normalized;

    normalized = policy;
    if (normalized == SIXEL_LUT_POLICY_AUTO) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    } else if (normalized != SIXEL_LUT_POLICY_5BIT
               && normalized != SIXEL_LUT_POLICY_6BIT
               && normalized != SIXEL_LUT_POLICY_CERTLUT
               && normalized != SIXEL_LUT_POLICY_NONE) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    }

    return normalized;
}

static float
sixel_lookup_float32_component(float const *palette,
                               int depth,
                               int index,
                               int axis)
{
    int clamped_axis;

    clamped_axis = axis;
    if (clamped_axis < 0) {
        clamped_axis = 0;
    } else if (clamped_axis >= SIXEL_LOOKUP_FLOAT_COMPONENTS) {
        clamped_axis = SIXEL_LOOKUP_FLOAT_COMPONENTS - 1;
    }

    return palette[index * depth + clamped_axis];
}

static void
sixel_lookup_float32_sort_indices(float const *palette,
                                  int depth,
                                  int *indices,
                                  int count,
                                  int axis)
{
    int i;
    int j;
    int key;
    float key_value;
    float current;

    for (i = 1; i < count; ++i) {
        key = indices[i];
        key_value = sixel_lookup_float32_component(palette,
                                                   depth,
                                                   key,
                                                   axis);
        j = i - 1;
        while (j >= 0) {
            current = sixel_lookup_float32_component(palette,
                                                     depth,
                                                     indices[j],
                                                     axis);
            if (current <= key_value) {
                break;
            }
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = key;
    }
}

static int
sixel_lookup_float32_build_kdtree(sixel_lookup_float32_t *lut,
                                  int *indices,
                                  int count,
                                  int depth)
{
    int axis;
    int median;
    int node_index;

    if (count <= 0) {
        return -1;
    }

    axis = depth % SIXEL_LOOKUP_FLOAT_COMPONENTS;
    sixel_lookup_float32_sort_indices(lut->palette,
                                      lut->depth,
                                      indices,
                                      count,
                                      axis);
    median = count / 2;
    node_index = lut->kdnodes_count;
    if (node_index >= lut->ncolors) {
        return -1;
    }

    lut->kdnodes_count++;
    lut->kdnodes[node_index].index = indices[median];
    lut->kdnodes[node_index].axis = axis;
    lut->kdnodes[node_index].left =
        sixel_lookup_float32_build_kdtree(lut,
                                          indices,
                                          median,
                                          depth + 1);
    lut->kdnodes[node_index].right =
        sixel_lookup_float32_build_kdtree(lut,
                                          indices + median + 1,
                                          count - median - 1,
                                          depth + 1);

    return node_index;
}

static float
sixel_lookup_float32_distance(sixel_lookup_float32_t const *lut,
                              float const *sample,
                              int palette_index)
{
    float diff;
    float distance;
    int component;

    distance = 0.0f;
    for (component = 0; component < SIXEL_LOOKUP_FLOAT_COMPONENTS;
            ++component) {
        diff = sample[component]
             - sixel_lookup_float32_component(lut->palette,
                                              lut->depth,
                                              palette_index,
                                              component);
        diff *= diff;
        diff *= (float)lut->weights[component];
        distance += diff;
    }

    return distance;
}

static void
sixel_lookup_float32_search_kdtree(sixel_lookup_float32_t const *lut,
                                   int node_index,
                                   float const *sample,
                                   int *best_index,
                                   float *best_distance)
{
    sixel_lookup_float32_node_t const *node;
    float pivot;
    float diff;
    float distance;
    float plane_distance;
    int next;
    int other;

    if (node_index < 0) {
        return;
    }

    node = &lut->kdnodes[node_index];
    pivot = sixel_lookup_float32_component(lut->palette,
                                           lut->depth,
                                           node->index,
                                           node->axis);
    diff = sample[node->axis] - pivot;
    next = node->left;
    other = node->right;
    if (diff > 0.0f) {
        next = node->right;
        other = node->left;
    }

    sixel_lookup_float32_search_kdtree(lut,
                                       next,
                                       sample,
                                       best_index,
                                       best_distance);

    distance = sixel_lookup_float32_distance(lut, sample, node->index);
    if (distance < *best_distance) {
        *best_distance = distance;
        *best_index = node->index;
    }

    plane_distance = diff * diff * (float)lut->weights[node->axis];
    if (plane_distance < *best_distance) {
        sixel_lookup_float32_search_kdtree(lut,
                                           other,
                                           sample,
                                           best_index,
                                           best_distance);
    }
}

static int
sixel_lookup_float32_linear_search(sixel_lookup_float32_t const *lut,
                                   float const *sample)
{
    float best;
    float distance;
    int best_index;
    int index;

    best = FLT_MAX;
    best_index = 0;
    for (index = 0; index < lut->ncolors; ++index) {
        distance = sixel_lookup_float32_distance(lut, sample, index);
        if (distance < best) {
            best = distance;
            best_index = index;
        }
    }

    return best_index;
}

void
sixel_lookup_float32_init(sixel_lookup_float32_t *lut,
                          sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(sixel_lookup_float32_t));
    lut->policy = SIXEL_LUT_POLICY_6BIT;
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
    lut->weights[0] = 1;
    lut->weights[1] = 1;
    lut->weights[2] = 1;
    lut->palette = NULL;
    lut->kdnodes = NULL;
    lut->kdtree_root = -1;
    lut->kdnodes_count = 0;
    lut->allocator = allocator;
}

static void
sixel_lookup_float32_release_palette(sixel_lookup_float32_t *lut)
{
    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }
}

static void
sixel_lookup_float32_release_kdtree(sixel_lookup_float32_t *lut)
{
    free(lut->kdnodes);
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
}

void
sixel_lookup_float32_clear(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_float32_release_palette(lut);
    sixel_lookup_float32_release_kdtree(lut);
    lut->ncolors = 0;
    lut->depth = 0;
}

void
sixel_lookup_float32_finalize(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_float32_clear(lut);
    lut->allocator = NULL;
}

static SIXELSTATUS
sixel_lookup_float32_prepare_palette(sixel_lookup_float32_t *lut,
                                     unsigned char const *palette,
                                     int pixelformat)
{
    size_t total;
    int index;
    int component;
    float *cursor;

    total = (size_t)lut->ncolors * (size_t)lut->depth;
    sixel_lookup_float32_release_palette(lut);
    lut->palette = (float *)sixel_allocator_malloc(lut->allocator,
                                                   total * sizeof(float));
    if (lut->palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_float32_prepare_palette: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    for (index = 0; index < lut->ncolors; ++index) {
        for (component = 0; component < lut->depth; ++component) {
            *cursor = sixel_pixelformat_byte_to_float(pixelformat,
                                                     component,
                                                     palette[index
                                                         * lut->depth
                                                         + component]);
            ++cursor;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_float32_prepare_kdtree(sixel_lookup_float32_t *lut)
{
    int *indices;
    SIXELSTATUS status;
    int i;

    lut->kdnodes = (sixel_lookup_float32_node_t *)
        calloc((size_t)lut->ncolors, sizeof(sixel_lookup_float32_node_t));
    if (lut->kdnodes == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    indices = (int *)malloc((size_t)lut->ncolors * sizeof(int));
    if (indices == NULL) {
        free(lut->kdnodes);
        lut->kdnodes = NULL;
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0; i < lut->ncolors; ++i) {
        indices[i] = i;
    }
    lut->kdnodes_count = 0;
    lut->kdtree_root = sixel_lookup_float32_build_kdtree(lut,
                                                          indices,
                                                          lut->ncolors,
                                                          0);
    status = SIXEL_OK;
    if (lut->kdtree_root < 0) {
        status = SIXEL_BAD_ALLOCATION;
    }

    free(indices);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_float32_release_kdtree(lut);
    }

    return status;
}

SIXELSTATUS
sixel_lookup_float32_configure(sixel_lookup_float32_t *lut,
                               unsigned char const *palette,
                               int depth,
                               int ncolors,
                               int complexion,
                               int wcomp1,
                               int wcomp2,
                               int wcomp3,
                               int policy,
                               int pixelformat)
{
    SIXELSTATUS status;

    (void)pixelformat;

    if (lut == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (depth <= 0 || ncolors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_float32_clear(lut);
    lut->policy = sixel_lookup_float32_policy_normalize(policy);
    lut->depth = depth;
    lut->ncolors = ncolors;
    lut->complexion = complexion;
    /*
     * Apply per-component weighting.  The first component inherits the
     * complexion factor to preserve existing luminance-driven defaults while
     * keeping the parameter names agnostic to the color space.
     */
    lut->weights[0] = wcomp1 * complexion;
    lut->weights[1] = wcomp2;
    lut->weights[2] = wcomp3;

    status = sixel_lookup_float32_prepare_palette(lut, palette, pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (lut->policy == SIXEL_LUT_POLICY_CERTLUT) {
        status = sixel_lookup_float32_prepare_kdtree(lut);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

int
sixel_lookup_float32_map_pixel(sixel_lookup_float32_t *lut,
                               unsigned char const *pixel)
{
    float const *sample;
    int best_index;
    float best_distance;

    if (lut == NULL || pixel == NULL) {
        return 0;
    }

    sample = (float const *)(void const *)pixel;
    if (lut->policy == SIXEL_LUT_POLICY_CERTLUT) {
        best_index = 0;
        best_distance = FLT_MAX;
        sixel_lookup_float32_search_kdtree(lut,
                                           lut->kdtree_root,
                                           sample,
                                           &best_index,
                                           &best_distance);
        return best_index;
    }

    return sixel_lookup_float32_linear_search(lut, sample);
}

