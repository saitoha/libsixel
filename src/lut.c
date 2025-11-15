/*
 * SPDX-License-Identifier: MIT
 *
 * mediancut algorithm implementation is imported from pnmcolormap.c
 * in netpbm library.
 * http://netpbm.sourceforge.net/
 *
 * *******************************************************************************
 *                  original license block of pnmcolormap.c
 * *******************************************************************************
 *
 *   Derived from ppmquant, originally by Jef Poskanzer.
 *
 *   Copyright (C) 1989, 1991 by Jef Poskanzer.
 *   Copyright (C) 2001 by Bryan Henderson.
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted, provided
 *   that the above copyright notice appear in all copies and that both that
 *   copyright notice and this permission notice appear in supporting
 *   documentation.  This software is provided "as is" without express or
 *   implied warranty.
 *
 * ******************************************************************************
 *
 * Copyright (c) 2014-2018 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The lookup table implementation manages colour quantization caches and the
 * CERT LUT accelerator.  Median-cut specific histogram routines now reside in
 * palette-heckbert.c so this file can concentrate on lookup responsibilities.
 */

#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#include <intrin.h>
#endif

#include <sixel.h>

#include "status.h"
#include "compat_stub.h"
#include "allocator.h"
#include "lut.h"

#define SIXEL_LUT_BRANCH_FLAG 0x40000000U
/* #define DEBUG_LUT_TRACE 1 */

typedef struct sixel_certlut_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} sixel_certlut_color_t;

typedef struct sixel_certlut_node {
    int index;
    int left;
    int right;
    unsigned char axis;
} sixel_certlut_node_t;

typedef struct sixel_certlut {
    uint32_t *level0;
    uint8_t *pool;
    uint32_t pool_size;
    uint32_t pool_capacity;
    int wR;
    int wG;
    int wB;
    uint64_t wR2;
    uint64_t wG2;
    uint64_t wB2;
    int32_t wr_scale[256];
    int32_t wg_scale[256];
    int32_t wb_scale[256];
    int32_t *wr_palette;
    int32_t *wg_palette;
    int32_t *wb_palette;
    sixel_certlut_color_t const *palette;
    int ncolors;
    sixel_certlut_node_t *kdnodes;
    int kdnodes_count;
    int kdtree_root;
} sixel_certlut_t;

typedef struct sixel_lut_quantization {
    unsigned int channel_shift;
    unsigned int channel_bits;
    unsigned int channel_mask;
} sixel_lut_quantization_t;

struct sixel_lut {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    unsigned char const *palette;
    sixel_allocator_t *allocator;
    sixel_lut_quantization_t quant;
    int32_t *dense;
    size_t dense_size;
    int dense_ready;
    sixel_certlut_t cert;
    int cert_ready;
};

/* Sentinel value used to detect empty dense LUT slots. */
#define SIXEL_LUT_DENSE_EMPTY (-1)

/*
 * Compute quantization parameters for the dense cache.  The selection mirrors
 * the historic histogram helper so existing 5bit/6bit behaviour stays intact
 * while still allowing "none" and "certlut" to bypass the cache entirely.
 */
static sixel_lut_quantization_t
sixel_lut_quant_make(unsigned int depth, int policy)
{
    sixel_lut_quantization_t quant;
    unsigned int shift;

    shift = 2U;
    if (depth > 3U) {
        shift = 3U;
    }
    if (policy == SIXEL_LUT_POLICY_5BIT) {
        shift = 3U;
    } else if (policy == SIXEL_LUT_POLICY_6BIT) {
        shift = 2U;
        if (depth > 3U) {
            shift = 3U;
        }
    } else if (policy == SIXEL_LUT_POLICY_NONE
               || policy == SIXEL_LUT_POLICY_CERTLUT) {
        shift = 0U;
    }

    quant.channel_shift = shift;
    quant.channel_bits = 8U - shift;
    quant.channel_mask = (1U << quant.channel_bits) - 1U;

    return quant;
}

/*
 * Return the dense table size for the active quantization.  The loop saturates
 * at SIZE_MAX so the later allocation guard can emit a friendly error message
 * instead of overflowing.
 */
static size_t
sixel_lut_dense_size(unsigned int depth,
                     sixel_lut_quantization_t const *quant)
{
    size_t size;
    unsigned int exponent;
    unsigned int i;

    size = 1U;
    exponent = depth * quant->channel_bits;
    for (i = 0U; i < exponent; ++i) {
        if (size > SIZE_MAX / 2U) {
            size = SIZE_MAX;
            break;
        }
        size <<= 1U;
    }

    return size;
}

/*
 * Pack a pixel into the dense cache index.  The rounding matches the old
 * histogram_pack_color() implementation to keep cached answers stable.
 */
static unsigned int
sixel_lut_pack_color(unsigned char const *pixel,
                     unsigned int depth,
                     sixel_lut_quantization_t const *quant)
{
    unsigned int packed;
    unsigned int bits;
    unsigned int shift;
    unsigned int plane;
    unsigned int component;
    unsigned int rounded;
    unsigned int mask;

    packed = 0U;
    bits = quant->channel_bits;
    shift = quant->channel_shift;
    mask = quant->channel_mask;

    for (plane = 0U; plane < depth; ++plane) {
        component = (unsigned int)pixel[depth - 1U - plane];
        if (shift > 0U) {
            rounded = (component + (1U << (shift - 1U))) >> shift;
            if (rounded > mask) {
                rounded = mask;
            }
        } else {
            rounded = component & mask;
        }
        packed |= rounded << (plane * bits);
    }

    return packed;
}

static int
sixel_lut_policy_normalize(int policy)
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

static int
sixel_lut_policy_uses_cache(int policy)
{
    if (policy == SIXEL_LUT_POLICY_CERTLUT
        || policy == SIXEL_LUT_POLICY_NONE) {
        return 0;
    }

    return 1;
}

static void
sixel_lut_release_cache(sixel_lut_t *lut)
{
    if (lut == NULL || lut->dense == NULL) {
        return;
    }

    sixel_allocator_free(lut->allocator, lut->dense);
    lut->dense = NULL;
    lut->dense_size = 0U;
    lut->dense_ready = 0;
}

static SIXELSTATUS
sixel_lut_prepare_cache(sixel_lut_t *lut)
{
    size_t expected;
    size_t bytes;
    size_t index;

    if (lut == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_lut_policy_uses_cache(lut->policy)) {
        return SIXEL_OK;
    }
    if (lut->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /* Allocate a dense RGB quantization table for 5/6bit policies.
     * The packed index matches sixel_lut_pack_color() so each slot
     * can store the resolved palette entry or remain empty.
     */
    expected = sixel_lut_dense_size((unsigned int)lut->depth,
                                    &lut->quant);
    if (expected == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (expected > SIZE_MAX / sizeof(int32_t)) {
        sixel_helper_set_additional_message(
            "sixel_lut_prepare_cache: dense table too large.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (lut->dense != NULL && lut->dense_size != expected) {
        sixel_lut_release_cache(lut);
    }

    if (lut->dense == NULL) {
        bytes = expected * sizeof(int32_t);
        lut->dense = (int32_t *)sixel_allocator_malloc(lut->allocator,
                                                       bytes);
        if (lut->dense == NULL) {
            sixel_helper_set_additional_message(
                "sixel_lut_prepare_cache: allocation failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    for (index = 0U; index < expected; ++index) {
        lut->dense[index] = SIXEL_LUT_DENSE_EMPTY;
    }

    lut->dense_size = expected;
    lut->dense_ready = 1;

    return SIXEL_OK;
}

static int
sixel_lut_lookup_fast(sixel_lut_t *lut, unsigned char const *pixel)
{
    int result;
    int diff;
    int i;
    int distant;
    unsigned char const *entry;
    unsigned char const *end;
    int pixel0;
    int pixel1;
    int pixel2;
    int delta;
    unsigned int bucket;
    int32_t cached;

    if (lut == NULL || pixel == NULL) {
        return 0;
    }
    if (lut->palette == NULL || lut->ncolors <= 0) {
        return 0;
    }

    bucket = 0U;
    cached = SIXEL_LUT_DENSE_EMPTY;
    if (lut->dense_ready && lut->dense != NULL) {
        bucket = sixel_lut_pack_color(pixel,
                                      (unsigned int)lut->depth,
                                      &lut->quant);
        if ((size_t)bucket < lut->dense_size) {
            cached = lut->dense[bucket];
            if (cached >= 0) {
                return cached;
            }
        }
    }

    result = (-1);
    diff = INT_MAX;
    /* Linear palette scan remains as a safety net when the dense
     * lookup does not have an answer yet.
     */
    entry = lut->palette;
    end = lut->palette + (size_t)lut->ncolors * (size_t)lut->depth;
    pixel0 = (int)pixel[0];
    pixel1 = (int)pixel[1];
    pixel2 = (int)pixel[2];
    for (i = 0; entry < end; ++i, entry += lut->depth) {
        delta = pixel0 - (int)entry[0];
        distant = delta * delta * lut->complexion;
        delta = pixel1 - (int)entry[1];
        distant += delta * delta;
        delta = pixel2 - (int)entry[2];
        distant += delta * delta;
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    if (lut->dense_ready && lut->dense != NULL && result >= 0) {
        if ((size_t)bucket < lut->dense_size) {
            lut->dense[bucket] = result;
        }
    }

    if (result < 0) {
        result = 0;
    }

    return result;
}

static int
sixel_certlut_init(sixel_certlut_t *lut)
{
    int status;

    status = SIXEL_FALSE;
    if (lut == NULL) {
        goto end;
    }

    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    lut->wR = 1;
    lut->wG = 1;
    lut->wB = 1;
    lut->wR2 = 1U;
    lut->wG2 = 1U;
    lut->wB2 = 1U;
    memset(lut->wr_scale, 0, sizeof(lut->wr_scale));
    memset(lut->wg_scale, 0, sizeof(lut->wg_scale));
    memset(lut->wb_scale, 0, sizeof(lut->wb_scale));
    lut->wr_palette = NULL;
    lut->wg_palette = NULL;
    lut->wb_palette = NULL;
    lut->palette = NULL;
    lut->ncolors = 0;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_certlut_release(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    free(lut->level0);
    free(lut->pool);
    free(lut->wr_palette);
    free(lut->wg_palette);
    free(lut->wb_palette);
    free(lut->kdnodes);
    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    lut->wr_palette = NULL;
    lut->wg_palette = NULL;
    lut->wb_palette = NULL;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
}

static int
sixel_certlut_prepare_palette_terms(sixel_certlut_t *lut)
{
    int status;
    size_t count;
    int index;
    int32_t *wr_terms;
    int32_t *wg_terms;
    int32_t *wb_terms;

    status = SIXEL_FALSE;
    wr_terms = NULL;
    wg_terms = NULL;
    wb_terms = NULL;
    if (lut == NULL) {
        goto end;
    }
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    count = (size_t)lut->ncolors;
    wr_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wr_terms == NULL) {
        goto end;
    }
    wg_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wg_terms == NULL) {
        goto end;
    }
    wb_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wb_terms == NULL) {
        goto end;
    }
    for (index = 0; index < lut->ncolors; ++index) {
        wr_terms[index] = lut->wR * (int)lut->palette[index].r;
        wg_terms[index] = lut->wG * (int)lut->palette[index].g;
        wb_terms[index] = lut->wB * (int)lut->palette[index].b;
    }
    free(lut->wr_palette);
    free(lut->wg_palette);
    free(lut->wb_palette);
    lut->wr_palette = wr_terms;
    lut->wg_palette = wg_terms;
    lut->wb_palette = wb_terms;
    wr_terms = NULL;
    wg_terms = NULL;
    wb_terms = NULL;
    status = SIXEL_OK;

end:
    free(wr_terms);
    free(wg_terms);
    free(wb_terms);
    return status;
}

static void
sixel_certlut_cell_center(int rmin, int gmin, int bmin, int size,
                        int *cr, int *cg, int *cb)
{
    int half;

    half = size / 2;
    *cr = rmin + half;
    *cg = gmin + half;
    *cb = bmin + half;
    if (size == 1) {
        *cr = rmin;
        *cg = gmin;
        *cb = bmin;
    }
}

static void
sixel_certlut_weight_init(sixel_certlut_t *lut, int wR, int wG, int wB)
{
    int i;

    lut->wR = wR;
    lut->wG = wG;
    lut->wB = wB;
    lut->wR2 = (uint64_t)wR * (uint64_t)wR;
    lut->wG2 = (uint64_t)wG * (uint64_t)wG;
    lut->wB2 = (uint64_t)wB * (uint64_t)wB;
    for (i = 0; i < 256; ++i) {
        lut->wr_scale[i] = wR * i;
        lut->wg_scale[i] = wG * i;
        lut->wb_scale[i] = wB * i;
    }
}

static uint64_t
sixel_certlut_distance_precomputed(sixel_certlut_t const *lut,
                                   int index,
                                   int32_t wr_r,
                                   int32_t wg_g,
                                   int32_t wb_b)
{
    uint64_t distance;
    int64_t diff;

    diff = (int64_t)wr_r - (int64_t)lut->wr_palette[index];
    distance = (uint64_t)(diff * diff);
    diff = (int64_t)wg_g - (int64_t)lut->wg_palette[index];
    distance += (uint64_t)(diff * diff);
    diff = (int64_t)wb_b - (int64_t)lut->wb_palette[index];
    distance += (uint64_t)(diff * diff);

    return distance;
}

static int
sixel_certlut_is_cell_safe(sixel_certlut_t const *lut, int best_idx,
                         int second_idx, int size, uint64_t best_dist,
                         uint64_t second_dist)
{
    uint64_t delta_sq;
    uint64_t rhs;
    uint64_t weight_term;
    int64_t wr_delta;
    int64_t wg_delta;
    int64_t wb_delta;

    if (best_idx < 0 || second_idx < 0) {
        return 1;
    }

    /*
     * The certification bound compares the squared distance gap against the
     * palette separation scaled by the cube diameter.  If the gap wins the
     * entire cube maps to the current best palette entry.
     */
    delta_sq = second_dist - best_dist;
    wr_delta = (int64_t)lut->wr_palette[second_idx]
        - (int64_t)lut->wr_palette[best_idx];
    wg_delta = (int64_t)lut->wg_palette[second_idx]
        - (int64_t)lut->wg_palette[best_idx];
    wb_delta = (int64_t)lut->wb_palette[second_idx]
        - (int64_t)lut->wb_palette[best_idx];
    weight_term = (uint64_t)(wr_delta * wr_delta);
    weight_term += (uint64_t)(wg_delta * wg_delta);
    weight_term += (uint64_t)(wb_delta * wb_delta);
    rhs = (uint64_t)3 * (uint64_t)size * (uint64_t)size * weight_term;

    return delta_sq * delta_sq > rhs;
}

static uint32_t
sixel_certlut_pool_alloc(sixel_certlut_t *lut, int *status)
{
    uint32_t required;
    uint32_t next_capacity;
    uint32_t offset;
    uint8_t *resized;

    offset = 0U;
    if (status != NULL) {
        *status = SIXEL_FALSE;
    }
    required = lut->pool_size + (uint32_t)(8 * sizeof(uint32_t));
    if (required > lut->pool_capacity) {
        next_capacity = lut->pool_capacity;
        if (next_capacity == 0U) {
            next_capacity = (uint32_t)(8 * sizeof(uint32_t));
        }
        while (next_capacity < required) {
            if (next_capacity > UINT32_MAX / 2U) {
                return 0U;
            }
            next_capacity *= 2U;
        }
        resized = (uint8_t *)realloc(lut->pool, next_capacity);
        if (resized == NULL) {
            return 0U;
        }
        lut->pool = resized;
        lut->pool_capacity = next_capacity;
    }
    offset = lut->pool_size;
    memset(lut->pool + offset, 0, 8 * sizeof(uint32_t));
    lut->pool_size = required;
    if (status != NULL) {
        *status = SIXEL_OK;
    }

    return offset;
}

static void
sixel_certlut_assign_leaf(uint32_t *cell, int palette_index)
{
    *cell = 0x80000000U | (uint32_t)(palette_index & 0xff);
}

static void
sixel_certlut_assign_branch(uint32_t *cell, uint32_t offset)
{
    *cell = SIXEL_LUT_BRANCH_FLAG | (offset & 0x3fffffffU);
}

static int
sixel_certlut_palette_component(sixel_certlut_t const *lut,
                                int index, int axis)
{
    sixel_certlut_color_t const *color;

    color = &lut->palette[index];
    if (axis == 0) {
        return (int)color->r;
    }
    if (axis == 1) {
        return (int)color->g;
    }
    return (int)color->b;
}

static void
sixel_certlut_sort_indices(sixel_certlut_t const *lut,
                           int *indices, int count, int axis)
{
    int i;
    int j;
    int key;
    int key_value;
    int current_value;

    for (i = 1; i < count; ++i) {
        key = indices[i];
        key_value = sixel_certlut_palette_component(lut, key, axis);
        j = i - 1;
        while (j >= 0) {
            current_value = sixel_certlut_palette_component(lut,
                                                            indices[j],
                                                            axis);
            if (current_value <= key_value) {
                break;
            }
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = key;
    }
}

static int
sixel_certlut_kdtree_build_recursive(sixel_certlut_t *lut,
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

    axis = depth % 3;
    sixel_certlut_sort_indices(lut, indices, count, axis);
    median = count / 2;
    node_index = lut->kdnodes_count;
    if (node_index >= lut->ncolors) {
        return -1;
    }
    lut->kdnodes_count++;
    lut->kdnodes[node_index].index = indices[median];
    lut->kdnodes[node_index].axis = (unsigned char)axis;
    lut->kdnodes[node_index].left =
        sixel_certlut_kdtree_build_recursive(lut,
                                             indices,
                                             median,
                                             depth + 1);
    lut->kdnodes[node_index].right =
        sixel_certlut_kdtree_build_recursive(lut,
                                             indices + median + 1,
                                             count - median - 1,
                                             depth + 1);

    return node_index;
}

static SIXELSTATUS
sixel_certlut_kdtree_build(sixel_certlut_t *lut)
{
    SIXELSTATUS status;
    int *indices;
    int i;

    status = SIXEL_FALSE;
    indices = NULL;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    lut->kdnodes = (sixel_certlut_node_t *)
        calloc((size_t)lut->ncolors, sizeof(sixel_certlut_node_t));
    if (lut->kdnodes == NULL) {
        goto end;
    }
    indices = (int *)malloc((size_t)lut->ncolors * sizeof(int));
    if (indices == NULL) {
        goto end;
    }
    for (i = 0; i < lut->ncolors; ++i) {
        indices[i] = i;
    }
    lut->kdnodes_count = 0;
    lut->kdtree_root = sixel_certlut_kdtree_build_recursive(lut,
                                                            indices,
                                                            lut->ncolors,
                                                            0);
    if (lut->kdtree_root < 0) {
        goto end;
    }
    status = SIXEL_OK;

end:
    free(indices);
    if (SIXEL_FAILED(status)) {
        free(lut->kdnodes);
        lut->kdnodes = NULL;
        lut->kdnodes_count = 0;
        lut->kdtree_root = -1;
    }

    return status;
}

static uint64_t
sixel_certlut_axis_distance(sixel_certlut_t const *lut, int diff, int axis)
{
    uint64_t weight;
    uint64_t abs_diff;

    abs_diff = (uint64_t)(diff < 0 ? -diff : diff);
    if (axis == 0) {
        weight = lut->wR2;
    } else if (axis == 1) {
        weight = lut->wG2;
    } else {
        weight = lut->wB2;
    }

    return weight * abs_diff * abs_diff;
}

static void
sixel_certlut_consider_candidate(sixel_certlut_t const *lut,
                                 int candidate,
                                 int32_t wr_r,
                                 int32_t wg_g,
                                 int32_t wb_b,
                                 int *best_idx,
                                 uint64_t *best_dist,
                                 int *second_idx,
                                 uint64_t *second_dist)
{
    uint64_t distance;

    distance = sixel_certlut_distance_precomputed(lut,
                                                  candidate,
                                                  wr_r,
                                                  wg_g,
                                                  wb_b);
    if (distance < *best_dist) {
        *second_dist = *best_dist;
        *second_idx = *best_idx;
        *best_dist = distance;
        *best_idx = candidate;
    } else if (distance < *second_dist) {
        *second_dist = distance;
        *second_idx = candidate;
    }
}

static void
sixel_certlut_kdtree_search(sixel_certlut_t const *lut,
                            int node_index,
                            int r,
                            int g,
                            int b,
                            int32_t wr_r,
                            int32_t wg_g,
                            int32_t wb_b,
                            int *best_idx,
                            uint64_t *best_dist,
                            int *second_idx,
                            uint64_t *second_dist)
{
    sixel_certlut_node_t const *node;
    int axis;
    int value;
    int diff;
    int near_child;
    int far_child;
    uint64_t axis_bound;
    int component;

    if (node_index < 0) {
        return;
    }
    node = &lut->kdnodes[node_index];
    sixel_certlut_consider_candidate(lut,
                                     node->index,
                                     wr_r,
                                     wg_g,
                                     wb_b,
                                     best_idx,
                                     best_dist,
                                     second_idx,
                                     second_dist);

    axis = (int)node->axis;
    value = sixel_certlut_palette_component(lut, node->index, axis);
    if (axis == 0) {
        component = r;
    } else if (axis == 1) {
        component = g;
    } else {
        component = b;
    }
    diff = component - value;
    if (diff <= 0) {
        near_child = node->left;
        far_child = node->right;
    } else {
        near_child = node->right;
        far_child = node->left;
    }
    if (near_child >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    near_child,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
    axis_bound = sixel_certlut_axis_distance(lut, diff, axis);
    if (far_child >= 0 && axis_bound <= *second_dist) {
        sixel_certlut_kdtree_search(lut,
                                    far_child,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
}

static void
sixel_certlut_distance_pair(sixel_certlut_t const *lut, int r, int g, int b,
                            int *best_idx, int *second_idx,
                            uint64_t *best_dist, uint64_t *second_dist)
{
    int i;
    int best_candidate;
    int second_candidate;
    uint64_t best_value;
    uint64_t second_value;
    uint64_t distance;
    int rr;
    int gg;
    int bb;
    int32_t wr_r;
    int32_t wg_g;
    int32_t wb_b;

    best_candidate = (-1);
    second_candidate = (-1);
    best_value = UINT64_MAX;
    second_value = UINT64_MAX;
    rr = r;
    gg = g;
    bb = b;
    if (rr < 0) {
        rr = 0;
    } else if (rr > 255) {
        rr = 255;
    }
    if (gg < 0) {
        gg = 0;
    } else if (gg > 255) {
        gg = 255;
    }
    if (bb < 0) {
        bb = 0;
    } else if (bb > 255) {
        bb = 255;
    }
    wr_r = lut->wr_scale[rr];
    wg_g = lut->wg_scale[gg];
    wb_b = lut->wb_scale[bb];
    if (lut->kdnodes != NULL && lut->kdtree_root >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    lut->kdtree_root,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    &best_candidate,
                                    &best_value,
                                    &second_candidate,
                                    &second_value);
    } else {
        for (i = 0; i < lut->ncolors; ++i) {
            distance = sixel_certlut_distance_precomputed(lut,
                                                          i,
                                                          wr_r,
                                                          wg_g,
                                                          wb_b);
            if (distance < best_value) {
                second_value = best_value;
                second_candidate = best_candidate;
                best_value = distance;
                best_candidate = i;
            } else if (distance < second_value) {
                second_value = distance;
                second_candidate = i;
            }
        }
    }
    if (second_candidate < 0) {
        second_candidate = best_candidate;
        second_value = best_value;
    }
    *best_idx = best_candidate;
    *second_idx = second_candidate;
    *best_dist = best_value;
    *second_dist = second_value;
}

static uint8_t
sixel_certlut_fallback(sixel_certlut_t const *lut, int r, int g, int b)
{
    int best_idx;
    int second_idx;
    uint64_t best_dist;
    uint64_t second_dist;

    best_idx = -1;
    second_idx = -1;
    best_dist = 0U;
    second_dist = 0U;
    if (lut == NULL) {
        return 0U;
    }
    /*
     * The lazy builder may fail when allocations run out.  Fall back to a
     * direct brute-force palette search so lookups still succeed even in low
     * memory conditions.
     */
    sixel_certlut_distance_pair(lut,
                                r,
                                g,
                                b,
                                &best_idx,
                                &second_idx,
                                &best_dist,
                                &second_dist);
    if (best_idx < 0) {
        return 0U;
    }

    return (uint8_t)best_idx;
}

SIXELSTATUS
sixel_certlut_build_cell(sixel_certlut_t *lut, uint32_t *cell,
                         int rmin, int gmin, int bmin, int size)
{
    SIXELSTATUS status;
    int cr;
    int cg;
    int cb;
    int best_idx;
    int second_idx;
    uint64_t best_dist;
    uint64_t second_dist;
    uint32_t offset;
    int branch_status;
    uint8_t *pool_before;
    size_t pool_size_before;
    uint32_t cell_offset;
    int cell_in_pool;

    if (cell == NULL || lut == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (*cell == 0U) {
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "build_cell rmin=%d gmin=%d bmin=%d size=%d\n",
                rmin,
                gmin,
                bmin,
                size);
#endif
    }
    if (*cell != 0U) {
        status = SIXEL_OK;
        goto end;
    }

    /*
     * Each node represents an axis-aligned cube in RGB space.  The builder
     * certifies the dominant palette index by checking the distance gap at
     * the cell center.  When certification fails the cube is split into eight
     * octants backed by a pool block.  Children remain unbuilt until lookups
     * descend into them, keeping the workload proportional to actual queries.
     */
    status = SIXEL_FALSE;
    sixel_certlut_cell_center(rmin, gmin, bmin, size, &cr, &cg, &cb);
    sixel_certlut_distance_pair(lut, cr, cg, cb, &best_idx, &second_idx,
                              &best_dist, &second_dist);
    if (best_idx < 0) {
        best_idx = 0;
    }
    if (size == 1) {
        sixel_certlut_assign_leaf(cell, best_idx);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "  leaf idx=%d\n",
                best_idx);
#endif
        status = SIXEL_OK;
        goto end;
    }
    if (sixel_certlut_is_cell_safe(lut, best_idx, second_idx, size,
                                 best_dist, second_dist)) {
        sixel_certlut_assign_leaf(cell, best_idx);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "  safe leaf idx=%d\n",
                best_idx);
#endif
        status = SIXEL_OK;
        goto end;
    }
    pool_before = lut->pool;
    pool_size_before = lut->pool_size;
    cell_in_pool = 0;
    cell_offset = 0U;
    /*
     * The pool may grow while building descendants.  Remember the caller's
     * offset so the cell pointer can be refreshed after realloc moves the
     * backing storage.
     */
    if (pool_before != NULL) {
        if ((uint8_t *)(void *)cell >= pool_before
                && (size_t)((uint8_t *)(void *)cell - pool_before)
                        < pool_size_before) {
            cell_in_pool = 1;
            cell_offset = (uint32_t)((uint8_t *)(void *)cell - pool_before);
        }
    }
    offset = sixel_certlut_pool_alloc(lut, &branch_status);
    if (branch_status != SIXEL_OK) {
        goto end;
    }
    if (cell_in_pool != 0) {
        cell = (uint32_t *)(void *)(lut->pool + cell_offset);
    }
    sixel_certlut_assign_branch(cell, offset);
#ifdef DEBUG_LUT_TRACE
    fprintf(stderr,
            "  branch offset=%u\n",
            offset);
#endif
    status = SIXEL_OK;

end:
    return status;
}

SIXELSTATUS
sixel_certlut_build(sixel_certlut_t *lut, sixel_certlut_color_t const *palette,
                    int ncolors, int wR, int wG, int wB)
{
    SIXELSTATUS status;
    int initialized;
    size_t level0_count;
    status = SIXEL_FALSE;
    initialized = sixel_certlut_init(lut);
    if (SIXEL_FAILED(initialized)) {
        goto end;
    }
    lut->palette = palette;
    lut->ncolors = ncolors;
    sixel_certlut_weight_init(lut, wR, wG, wB);
    status = sixel_certlut_prepare_palette_terms(lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_certlut_kdtree_build(lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    level0_count = (size_t)64 * (size_t)64 * (size_t)64;
    lut->level0 = (uint32_t *)calloc(level0_count, sizeof(uint32_t));
    if (lut->level0 == NULL) {
        goto end;
    }
    /*
     * Level 0 cells start uninitialized.  The lookup routine materializes
     * individual subtrees on demand so we avoid evaluating the entire
     * 64x64x64 grid upfront.
     */
    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_certlut_release(lut);
    }
    return status;
}

uint8_t
sixel_certlut_lookup(sixel_certlut_t *lut, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t entry;
    uint32_t offset;
    uint32_t index;
    uint32_t *children;
    uint32_t *cell;
    int shift;
    int child;
    int status;
    int size;
    int rmin;
    int gmin;
    int bmin;
    int step;
    if (lut == NULL || lut->level0 == NULL) {
        return 0U;
    }
    /*
     * Cells are created lazily.  A zero entry indicates an uninitialized
     * subtree, so the builder is invoked with the cube bounds of the current
     * traversal.  Should allocation fail we fall back to a direct brute-force
     * palette search for the queried pixel.
     */
    index = ((uint32_t)(r >> 2) << 12)
          | ((uint32_t)(g >> 2) << 6)
          | (uint32_t)(b >> 2);
    cell = lut->level0 + index;
    size = 4;
    rmin = (int)(r & 0xfc);
    gmin = (int)(g & 0xfc);
    bmin = (int)(b & 0xfc);
    entry = *cell;
    if (entry == 0U) {
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "lookup build level0 r=%u g=%u b=%u\n",
                (unsigned int)r,
                (unsigned int)g,
                (unsigned int)b);
#endif
        status = sixel_certlut_build_cell(lut, cell, rmin, gmin, bmin, size);
        if (SIXEL_FAILED(status)) {
            return sixel_certlut_fallback(lut,
                                          (int)r,
                                          (int)g,
                                          (int)b);
        }
        entry = *cell;
    }
    shift = 1;
    while ((entry & 0x80000000U) == 0U) {
        offset = entry & 0x3fffffffU;
        children = (uint32_t *)(void *)(lut->pool + offset);
        child = (((int)(r >> shift) & 1) << 2)
              | (((int)(g >> shift) & 1) << 1)
              | ((int)(b >> shift) & 1);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "descend child=%d size=%d offset=%u\n",
                child,
                size,
                offset);
#endif
        step = size / 2;
        if (step <= 0) {
            step = 1;
        }
        rmin += step * ((child >> 2) & 1);
        gmin += step * ((child >> 1) & 1);
        bmin += step * (child & 1);
        size = step;
        cell = children + (size_t)child;
        entry = *cell;
        if (entry == 0U) {
#ifdef DEBUG_LUT_TRACE
            fprintf(stderr,
                    "lookup build child size=%d rmin=%d gmin=%d bmin=%d\n",
                    size,
                    rmin,
                    gmin,
                    bmin);
#endif
            status = sixel_certlut_build_cell(lut,
                                              cell,
                                              rmin,
                                              gmin,
                                              bmin,
                                              size);
            if (SIXEL_FAILED(status)) {
                return sixel_certlut_fallback(lut,
                                              (int)r,
                                              (int)g,
                                              (int)b);
            }
            children = (uint32_t *)(void *)(lut->pool + offset);
            cell = children + (size_t)child;
            entry = *cell;
        }
        if (size == 1) {
            break;
        }
        if (shift == 0) {
            break;
        }
        --shift;
    }

    return (uint8_t)(entry & 0xffU);
}

void
sixel_certlut_free(sixel_certlut_t *lut)
{
    sixel_certlut_release(lut);
    if (lut != NULL) {
        lut->palette = NULL;
        lut->ncolors = 0;
    }
}

SIXELSTATUS
sixel_lut_new(sixel_lut_t **out,
              int policy,
              sixel_allocator_t *allocator)
{
    sixel_lut_t *lut;
    SIXELSTATUS status;

    if (out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut = (sixel_lut_t *)malloc(sizeof(sixel_lut_t));
    if (lut == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memset(lut, 0, sizeof(sixel_lut_t));
    lut->allocator = allocator;
    lut->policy = sixel_lut_policy_normalize(policy);
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
    lut->palette = NULL;
    lut->quant.channel_shift = 0U;
    lut->quant.channel_bits = 8U;
    lut->quant.channel_mask = 0xffU;
    lut->dense = NULL;
    lut->dense_size = 0U;
    lut->dense_ready = 0;
    lut->cert_ready = 0;
    status = sixel_certlut_init(&lut->cert);
    if (SIXEL_FAILED(status)) {
        free(lut);
        sixel_helper_set_additional_message(
            "sixel_lut_new: unable to initialize certlut state.");
        return status;
    }

    *out = lut;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_lut_configure(sixel_lut_t *lut,
                    unsigned char const *palette,
                    int depth,
                    int ncolors,
                    int complexion,
                    int wR,
                    int wG,
                    int wB,
                    int policy)
{
    SIXELSTATUS status;
    int normalized;

    if (lut == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (depth <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut->palette = palette;
    lut->depth = depth;
    lut->ncolors = ncolors;
    lut->complexion = complexion;
    normalized = sixel_lut_policy_normalize(policy);
    lut->policy = normalized;
    lut->quant = sixel_lut_quant_make((unsigned int)depth, normalized);
    lut->dense_ready = 0;
    lut->cert_ready = 0;

    if (sixel_lut_policy_uses_cache(normalized)) {
        if (depth != 3) {
            sixel_helper_set_additional_message(
                "sixel_lut_configure: fast LUT requires RGB pixels.");
            return SIXEL_BAD_ARGUMENT;
        }
        status = sixel_lut_prepare_cache(lut);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    } else {
        sixel_lut_release_cache(lut);
        status = SIXEL_OK;
    }

    if (normalized == SIXEL_LUT_POLICY_CERTLUT) {
        status = sixel_certlut_build(&lut->cert,
                                     (sixel_certlut_color_t const *)palette,
                                     ncolors,
                                     wR,
                                     wG,
                                     wB);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        lut->cert_ready = 1;
    }

    return SIXEL_OK;
}

int
sixel_lut_map_pixel(sixel_lut_t *lut, unsigned char const *pixel)
{
    if (lut == NULL || pixel == NULL) {
        return 0;
    }
    if (lut->policy == SIXEL_LUT_POLICY_CERTLUT) {
        if (!lut->cert_ready) {
            return 0;
        }
        return (int)sixel_certlut_lookup(&lut->cert,
                                         pixel[0],
                                         pixel[1],
                                         pixel[2]);
    }

    return sixel_lut_lookup_fast(lut, pixel);
}

void
sixel_lut_clear(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lut_release_cache(lut);
    if (lut->cert_ready) {
        sixel_certlut_free(&lut->cert);
        lut->cert_ready = 0;
    }
    lut->palette = NULL;
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
}

void
sixel_lut_unref(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lut_clear(lut);
    free(lut);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
