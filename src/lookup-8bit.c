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
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#include <intrin.h>
#endif

#include <sixel.h>

#include "status.h"
#include "lookup-common.h"
#include "lookup-fhedt-8bit.h"
#include "logger.h"
#include "sixel_atomic.h"
#include "threading.h"
#include "compat_stub.h"
#include "allocator.h"
#include "lookup-8bit.h"

#define SIXEL_LUT_BRANCH_FLAG 0x40000000U
/* #define DEBUG_LUT_TRACE 1 */

enum { SIXEL_CERTLUT_COMPONENTS = 3 };

#define SIXEL_LOOKUP_PACK_LINEAR 0
#define SIXEL_LOOKUP_PACK_MORTON 1
#define SIXEL_LOOKUP_PACK_HILBERT 2

#define SIXEL_LOOKUP_EYTZINGER_WINDOW 6

#if HAVE_BUILTIN_PREFETCH
# define SIXEL_LOOKUP_EYTZINGER_PREFETCH(base, index, count) \
    do { \
        if ((index) <= (count)) { \
            __builtin_prefetch((base) + (index), 0, 1); \
        } \
    } while (0)
#else
# define SIXEL_LOOKUP_EYTZINGER_PREFETCH(base, index, count) ((void)0)
#endif

typedef struct sixel_certlut_color {
    uint8_t comp[SIXEL_CERTLUT_COMPONENTS];
} sixel_certlut_color_t;

typedef struct sixel_certlut_node {
    int index;
    int left;
    int right;
    unsigned char axis;
} sixel_certlut_node_t;

struct sixel_certlut {
    uint32_t *level0;
    uint8_t *pool;
    uint32_t pool_size;
    uint32_t pool_capacity;
    int weights[SIXEL_CERTLUT_COMPONENTS];
    uint64_t weights_sq[SIXEL_CERTLUT_COMPONENTS];
    int32_t scales[SIXEL_CERTLUT_COMPONENTS][256];
    int32_t *weight_palette[SIXEL_CERTLUT_COMPONENTS];
    sixel_certlut_color_t const *palette;
    int ncolors;
    sixel_certlut_node_t *kdnodes;
    int kdnodes_count;
    int kdtree_root;
    sixel_mutex_t lock;
    int lock_ready;
};

typedef struct sixel_lookup_8bit_1d_eytzinger_pair {
    float key;
    int index;
} sixel_lookup_8bit_1d_eytzinger_pair_t;

static void
sixel_lookup_8bit_1d_eytzinger_log_event(
    int ncolors,
    char const *event)
{
    sixel_logger_t logger;
    SIXELSTATUS status;

    sixel_logger_init(&logger);
    status = sixel_logger_prepare_env(&logger);
    if (SIXEL_FAILED(status) || !logger.active) {
        sixel_logger_close(&logger);
        return;
    }

    sixel_logger_logf(&logger,
                      "eytzinger",
                      "eytzinger",
                      event,
                      ncolors);
    sixel_logger_close(&logger);
}

/* Sentinel value used to detect empty dense LUT slots. */
#define SIXEL_LUT_DENSE_EMPTY (-1)

/*
 * Compute quantization parameters for the dense cache.  The selection mirrors
 * the historic histogram helper so existing 5bit/6bit behaviour stays intact
 * while still allowing "none" and "certlut" to bypass the cache entirely.
 */
static sixel_lookup_8bit_quantization_t
sixel_lookup_8bit_quant_make(unsigned int depth, int policy)
{
    sixel_lookup_8bit_quantization_t quant;
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

static int
sixel_lookup_8bit_env_packing(void)
{
    char const *env;

    env = sixel_compat_getenv("SIXEL_LOOKUP_PACKING");
    if (env == NULL || env[0] == '\0') {
        return SIXEL_LOOKUP_PACK_LINEAR;
    }

    if (sixel_compat_strcasecmp(env, "linear") == 0) {
        return SIXEL_LOOKUP_PACK_LINEAR;
    }
    if (sixel_compat_strcasecmp(env, "morton") == 0) {
        return SIXEL_LOOKUP_PACK_MORTON;
    }
    if (sixel_compat_strcasecmp(env, "hilbert") == 0) {
        return SIXEL_LOOKUP_PACK_HILBERT;
    }

    return SIXEL_LOOKUP_PACK_LINEAR;
}

/*
 * Return the dense table size for the active quantization.  The loop saturates
 * at SIZE_MAX so the later allocation guard can emit a friendly error message
 * instead of overflowing.
 */
static size_t
sixel_lookup_8bit_dense_size(
    unsigned int depth,
    sixel_lookup_8bit_quantization_t const *quant
)
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
sixel_lookup_8bit_pack_color_linear(
    unsigned char const *pixel,
    unsigned int depth,
    sixel_lookup_8bit_quantization_t const *quant
)
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

/*
 * Morton packing interleaves quantized channel bits so nearby RGB samples map
 * to nearby buckets.  The channel order mirrors the linear packing by using
 * the same component ordering (last component is plane 0).
 *
 * Layout example for 3 channels (bit 0 is LSB of each component):
 *   bit 0: C0(0) C1(0) C2(0)
 *   bit 1: C0(1) C1(1) C2(1)
 *   ...
 */
static unsigned int
sixel_lookup_8bit_pack_color_morton(
    unsigned char const *pixel,
    unsigned int depth,
    sixel_lookup_8bit_quantization_t const *quant
)
{
    unsigned int packed;
    unsigned int bits;
    unsigned int shift;
    unsigned int mask;
    unsigned int component;
    unsigned int rounded;
    unsigned int plane;
    unsigned int bit;
    unsigned int bit_index;
    unsigned int values[4];

    packed = 0U;
    bits = quant->channel_bits;
    shift = quant->channel_shift;
    mask = quant->channel_mask;

    if (depth == 0U) {
        return 0U;
    }
    if (depth > 4U || bits == 0U || bits * depth > 32U) {
        return sixel_lookup_8bit_pack_color_linear(pixel, depth, quant);
    }

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
        values[plane] = rounded;
    }

    for (bit = 0U; bit < bits; ++bit) {
        for (plane = 0U; plane < depth; ++plane) {
            bit_index = bit * depth + plane;
            packed |= ((values[plane] >> bit) & 1U) << bit_index;
        }
    }

    return packed;
}

/*
 * Hilbert packing preserves locality better than Morton in some workloads.
 * The Skilling transform applies rotation/reflection before interleaving
 * bits to produce the 3D Hilbert index.
 */
static unsigned int
sixel_lookup_8bit_pack_color_hilbert(
    unsigned char const *pixel,
    unsigned int depth,
    sixel_lookup_8bit_quantization_t const *quant
)
{
    unsigned int packed;
    unsigned int bits;
    unsigned int shift;
    unsigned int mask;
    unsigned int component;
    unsigned int rounded;
    unsigned int plane;
    unsigned int bit;
    unsigned int bit_index;
    unsigned int q;
    unsigned int p;
    unsigned int t;
    unsigned int coords[3];

    packed = 0U;
    bits = quant->channel_bits;
    shift = quant->channel_shift;
    mask = quant->channel_mask;

    if (depth != 3U || bits == 0U || bits * depth > 30U) {
        return sixel_lookup_8bit_pack_color_linear(pixel, depth, quant);
    }

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
        coords[plane] = rounded;
    }

    q = 1U << (bits - 1U);
    while (q > 1U) {
        p = q - 1U;
        for (plane = 0U; plane < depth; ++plane) {
            if ((coords[plane] & q) != 0U) {
                coords[0] ^= p;
            } else {
                t = (coords[0] ^ coords[plane]) & p;
                coords[0] ^= t;
                coords[plane] ^= t;
            }
        }
        q >>= 1U;
    }

    for (plane = 1U; plane < depth; ++plane) {
        coords[plane] ^= coords[plane - 1U];
    }

    t = 0U;
    q = 1U << (bits - 1U);
    while (q > 1U) {
        if ((coords[depth - 1U] & q) != 0U) {
            t ^= q - 1U;
        }
        q >>= 1U;
    }

    for (plane = 0U; plane < depth; ++plane) {
        coords[plane] ^= t;
    }

    for (bit = 0U; bit < bits; ++bit) {
        for (plane = 0U; plane < depth; ++plane) {
            bit_index = bit * depth + plane;
            packed |= ((coords[plane] >> bit) & 1U) << bit_index;
        }
    }

    return packed;
}

static unsigned int
sixel_lookup_8bit_pack_color(
    sixel_lookup_8bit_t const *lut,
    unsigned char const *pixel
)
{
    if (lut == NULL) {
        return 0U;
    }

    if (lut->packing == SIXEL_LOOKUP_PACK_MORTON) {
        return sixel_lookup_8bit_pack_color_morton(pixel,
                                                   (unsigned int)lut->depth,
                                                   &lut->quant);
    }
    if (lut->packing == SIXEL_LOOKUP_PACK_HILBERT) {
        return sixel_lookup_8bit_pack_color_hilbert(pixel,
                                                    (unsigned int)lut->depth,
                                                    &lut->quant);
    }

    return sixel_lookup_8bit_pack_color_linear(pixel,
                                               (unsigned int)lut->depth,
                                               &lut->quant);
}

static int
sixel_lookup_8bit_policy_normalize(int policy)
{
    int normalized;

    normalized = policy;
    if (normalized == SIXEL_LUT_POLICY_AUTO) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    } else if (normalized != SIXEL_LUT_POLICY_5BIT
               && normalized != SIXEL_LUT_POLICY_6BIT
               && normalized != SIXEL_LUT_POLICY_CERTLUT
               && normalized != SIXEL_LUT_POLICY_EYTZINGER
               && normalized != SIXEL_LUT_POLICY_NONE
               && normalized != SIXEL_LUT_POLICY_FHEDT
               && normalized != SIXEL_LUT_POLICY_VPTREE
               && normalized != SIXEL_LUT_POLICY_RBC
               && normalized != SIXEL_LUT_POLICY_MAHALANOBIS) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    }

    return normalized;
}

static int
sixel_lookup_8bit_policy_uses_cache(int policy)
{
    if (policy == SIXEL_LUT_POLICY_CERTLUT
        || policy == SIXEL_LUT_POLICY_EYTZINGER
        || policy == SIXEL_LUT_POLICY_NONE
        || policy == SIXEL_LUT_POLICY_FHEDT
        || policy == SIXEL_LUT_POLICY_VPTREE
        || policy == SIXEL_LUT_POLICY_RBC
        || policy == SIXEL_LUT_POLICY_MAHALANOBIS) {
        return 0;
    }

    return 1;
}

static int sixel_lookup_fhedt_env_resolution_8bit(void);
static int sixel_lookup_fhedt_env_refine_8bit(void);
static int sixel_lookup_fhedt_env_shared_8bit(void);
static int sixel_lookup_fhedt_env_use_dist2_8bit(void);
static int sixel_lookup_fhedt_env_use_cache_8bit(void);
 
static void
sixel_lookup_8bit_1d_eytzinger_release(sixel_lookup_8bit_t *lut)
{
    sixel_lookup_8bit_1d_eytzinger_t *eytz;

    if (lut == NULL) {
        return;
    }

    eytz = &lut->eytz;
    if (eytz->keys != NULL) {
        sixel_allocator_free(lut->allocator, eytz->keys);
    }
    if (eytz->palette_index != NULL) {
        sixel_allocator_free(lut->allocator, eytz->palette_index);
    }
    if (eytz->rank != NULL) {
        sixel_allocator_free(lut->allocator, eytz->rank);
    }
    if (eytz->sorted_palette_index != NULL) {
        sixel_allocator_free(lut->allocator, eytz->sorted_palette_index);
    }
    if (eytz->sorted_keys != NULL) {
        sixel_allocator_free(lut->allocator, eytz->sorted_keys);
    }

    eytz->keys = NULL;
    eytz->palette_index = NULL;
    eytz->rank = NULL;
    eytz->sorted_palette_index = NULL;
    eytz->sorted_keys = NULL;
    eytz->count = 0;
    eytz->ready = 0;
}

static float
sixel_lookup_8bit_1d_eytzinger_project(
    sixel_lookup_8bit_t const *lut,
    unsigned char const *pixel)
{
    int depth;
    int comp0;
    int comp1;
    int comp2;
    float key;

    depth = lut->depth;
    comp0 = (depth > 0) ? (int)pixel[0] : 0;
    comp1 = (depth > 1) ? (int)pixel[1] : 0;
    comp2 = (depth > 2) ? (int)pixel[2] : 0;

    key = (float)lut->eytz.weights[0] * (float)comp0
        + (float)lut->eytz.weights[1] * (float)comp1
        + (float)lut->eytz.weights[2] * (float)comp2;

    return key;
}

static int
sixel_lookup_8bit_1d_eytzinger_compare(void const *left, void const *right)
{
    float diff;
    sixel_lookup_8bit_1d_eytzinger_pair_t const *a;
    sixel_lookup_8bit_1d_eytzinger_pair_t const *b;

    a = (sixel_lookup_8bit_1d_eytzinger_pair_t const *)left;
    b = (sixel_lookup_8bit_1d_eytzinger_pair_t const *)right;
    diff = a->key - b->key;
    if (diff < 0.0f) {
        return -1;
    }
    if (diff > 0.0f) {
        return 1;
    }
    return 0;
}

static void
sixel_lookup_8bit_1d_eytzinger_fill(
    sixel_lookup_8bit_1d_eytzinger_t *eytz,
    sixel_lookup_8bit_1d_eytzinger_pair_t const *src,
    int count,
    int node,
    int *rank)
{
    if (node > count) {
        return;
    }

    sixel_lookup_8bit_1d_eytzinger_fill(eytz, src, count, node * 2, rank);
    eytz->keys[node] = src[*rank].key;
    eytz->palette_index[node] = src[*rank].index;
    eytz->rank[node] = *rank;
    (*rank)++;
    sixel_lookup_8bit_1d_eytzinger_fill(eytz, src, count, node * 2 + 1, rank);
}

static SIXELSTATUS
sixel_lookup_8bit_configure_1d_eytzinger(
    sixel_lookup_8bit_t *lut,
    unsigned char const *palette,
    int ncolors,
    int wcomp1,
    int wcomp2,
    int wcomp3)
{
    SIXELSTATUS status;
    sixel_lookup_8bit_1d_eytzinger_t *eytz;
    sixel_lookup_8bit_1d_eytzinger_pair_t *pairs;
    size_t bytes;
    int depth;
    int count;
    int index;
    int rank;

    status = SIXEL_BAD_ALLOCATION;
    eytz = &lut->eytz;
    pairs = NULL;

    sixel_lookup_8bit_1d_eytzinger_release(lut);
    eytz->ready = 0;
    eytz->count = 0;
    eytz->window = SIXEL_LOOKUP_EYTZINGER_WINDOW;
    eytz->weights[0] = wcomp1;
    eytz->weights[1] = wcomp2;
    eytz->weights[2] = wcomp3;

    if (palette == NULL || ncolors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    depth = lut->depth;
    count = ncolors;
    bytes = (size_t)count * sizeof(*pairs);
    pairs = (sixel_lookup_8bit_1d_eytzinger_pair_t *)sixel_allocator_malloc(
        lut->allocator,
        bytes);
    if (pairs == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_8bit_configure: Eytzinger allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    /*
     * Eytzinger preparation steps:
     *   1) Project palette entries onto a 1D key.
     *   2) Sort the (key, palette index) pairs by key.
     *   3) Reorder into an implicit binary tree while keeping ranks.
     */
    for (index = 0; index < count; ++index) {
        pairs[index].index = index;
        pairs[index].key = sixel_lookup_8bit_1d_eytzinger_project(
            lut,
            palette + (size_t)index * (size_t)depth);
    }

    qsort(pairs, (size_t)count, sizeof(*pairs),
          sixel_lookup_8bit_1d_eytzinger_compare);

    sixel_lookup_8bit_1d_eytzinger_log_event(count, "builder-start");

    eytz->keys = (float *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)(count + 1) * sizeof(float));
    eytz->palette_index = (int *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)(count + 1) * sizeof(int));
    eytz->rank = (int *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)(count + 1) * sizeof(int));
    eytz->sorted_palette_index = (int *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)count * sizeof(int));
    eytz->sorted_keys = (float *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)count * sizeof(float));

    if (eytz->keys == NULL || eytz->palette_index == NULL
            || eytz->rank == NULL || eytz->sorted_palette_index == NULL
            || eytz->sorted_keys == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_8bit_configure: Eytzinger arrays missing.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }

    for (index = 0; index < count; ++index) {
        eytz->sorted_palette_index[index] = pairs[index].index;
        eytz->sorted_keys[index] = pairs[index].key;
    }

    /*
     * Eytzinger layout in BFS order uses an inorder fill of the sorted array.
     * The table below shows how ranks map to nodes for n=7:
     *
     *   rank: 0 1 2 3 4 5 6
     *   node: 4 2 5 1 6 3 7
     */
    rank = 0;
    sixel_lookup_8bit_1d_eytzinger_fill(eytz, pairs, count, 1, &rank);

    eytz->count = count;
    eytz->ready = 1;
    sixel_lookup_8bit_1d_eytzinger_log_event(count, "builder-end");
    status = SIXEL_OK;

error:
    if (pairs != NULL) {
        sixel_allocator_free(lut->allocator, pairs);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_8bit_1d_eytzinger_release(lut);
    }
    return status;
}

static int
sixel_lookup_8bit_1d_eytzinger_lower_bound(
    sixel_lookup_8bit_1d_eytzinger_t const *eytz,
    float key)
{
    int node;
    int candidate;
    int count;
    int next;

    count = eytz->count;
    node = 1;
    candidate = 0;
    while (node <= count) {
        if (key <= eytz->keys[node]) {
            candidate = node;
            next = node * 2;
        } else {
            next = node * 2 + 1;
        }
        node = next;
        /*
         * Prefetch both the key array and the rank array so the next
         * iteration and the post-loop rank lookup can overlap memory fetches.
         */
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->keys, node, count);
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->keys, node + 1, count);
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->rank, node, count);
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->rank, node + 1, count);
    }
    return candidate;
}

static int
sixel_lookup_8bit_1d_eytzinger_distance(
    sixel_lookup_8bit_t const *lut,
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

    depth = lut->depth;
    pixel0 = (depth > 0) ? (int)pixel[0] : 0;
    pixel1 = (depth > 1) ? (int)pixel[1] : 0;
    pixel2 = (depth > 2) ? (int)pixel[2] : 0;
    entry = lut->palette + (size_t)palette_index * (size_t)depth;
    entry0 = (depth > 0) ? (int)entry[0] : 0;
    entry1 = (depth > 1) ? (int)entry[1] : 0;
    entry2 = (depth > 2) ? (int)entry[2] : 0;

    diff = pixel0 - entry0;
    distance = diff * diff * lut->complexion;
    diff = pixel1 - entry1;
    distance += diff * diff;
    diff = pixel2 - entry2;
    distance += diff * diff;

    return distance;
}

static int
sixel_lookup_8bit_lookup_1d_eytzinger(
    sixel_lookup_8bit_t *lut,
    unsigned char const *pixel)
{
    sixel_lookup_8bit_1d_eytzinger_t const *eytz;
    float key;
    int count;
    int node;
    int rank;
    int window;
    int start;
    int end;
    int offset_left;
    int offset_right;
    int stop_left;
    int stop_right;
    int palette_index;
    int best_index;
    int best_distance;
    int distance;
    float best_distance_f;
    float key_diff;
    float key_diff_sq;

    eytz = &lut->eytz;
    if (eytz->ready == 0 || eytz->count <= 0) {
        return 0;
    }

    key = sixel_lookup_8bit_1d_eytzinger_project(lut, pixel);
    node = sixel_lookup_8bit_1d_eytzinger_lower_bound(eytz, key);
    count = eytz->count;
    if (node == 0) {
        rank = count - 1;
    } else {
        rank = eytz->rank[node];
    }
    window = eytz->window;
    start = rank - window;
    if (start < 0) {
        start = 0;
    }
    end = rank + window;
    if (end >= count) {
        end = count - 1;
    }

    best_index = eytz->sorted_palette_index[rank];
    best_distance = sixel_lookup_8bit_1d_eytzinger_distance(lut,
                                                            pixel,
                                                            best_index);
    best_distance_f = (float)best_distance;
    offset_left = rank - 1;
    offset_right = rank + 1;
    stop_left = 0;
    stop_right = 0;
    /*
     * Use the key projection to short-circuit the neighbor scan. Within the
     * fixed window, we can stop searching a side once the squared key delta
     * exceeds the best distance found so far.
     */
    while (stop_left == 0 || stop_right == 0) {
        if (stop_left == 0) {
            if (offset_left < start) {
                stop_left = 1;
            } else {
                key_diff = key - eytz->sorted_keys[offset_left];
                key_diff_sq = key_diff * key_diff;
                if (key_diff_sq > best_distance_f) {
                    stop_left = 1;
                } else {
                    palette_index = eytz->sorted_palette_index[offset_left];
                    distance = sixel_lookup_8bit_1d_eytzinger_distance(
                        lut,
                        pixel,
                        palette_index);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_distance_f = (float)best_distance;
                        best_index = palette_index;
                    }
                    offset_left--;
                }
            }
        }
        if (stop_right == 0) {
            if (offset_right > end) {
                stop_right = 1;
            } else {
                key_diff = key - eytz->sorted_keys[offset_right];
                key_diff_sq = key_diff * key_diff;
                if (key_diff_sq > best_distance_f) {
                    stop_right = 1;
                } else {
                    palette_index = eytz->sorted_palette_index[offset_right];
                    distance = sixel_lookup_8bit_1d_eytzinger_distance(
                        lut,
                        pixel,
                        palette_index);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_distance_f = (float)best_distance;
                        best_index = palette_index;
                    }
                    offset_right++;
                }
            }
        }
    }

    return best_index;
}

static SIXELSTATUS
sixel_lookup_8bit_configure_fhedt(sixel_lookup_8bit_t *lut,
                                 unsigned char const *palette,
                                 int ncolors,
                                 int complexion,
                                 int wcomp1,
                                 int wcomp2,
                                 int wcomp3,
                                 int pixelformat)
{
    SIXELSTATUS status;
    int resolution;
    int refine;
    int shared_flag;
    int use_dist2;
    int use_cache;
    uint32_t signature;

    (void)complexion;

    resolution = sixel_lookup_fhedt_env_resolution_8bit();
    refine = sixel_lookup_fhedt_env_refine_8bit();
    shared_flag = sixel_lookup_fhedt_env_shared_8bit();
    use_dist2 = sixel_lookup_fhedt_env_use_dist2_8bit();
    use_cache = sixel_lookup_fhedt_env_use_cache_8bit();
 
    if (lut->fhedt == NULL) {
        status = sixel_lookup_fhedt_8bit_create(lut->allocator, &lut->fhedt);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_8bit_configure: FHEDT handle allocation failed.");
            return status;
        }
    }

    signature = sixel_lookup_fhedt_8bit_signature(palette,
                                                 ncolors,
                                                 resolution,
                                                 refine,
                                                 wcomp1,
                                                 wcomp2,
                                                 wcomp3,
                                                 lut->depth);

    status = sixel_lookup_fhedt_8bit_configure(lut->fhedt,
                                              palette,
                                              ncolors,
                                              resolution,
                                              refine,
                                              use_dist2,
                                              use_cache,
                                              shared_flag,
                                              wcomp1,
                                              wcomp2,
                                              wcomp3,
                                              pixelformat,
                                              lut->depth);
    if (SIXEL_FAILED(status)) {
        lut->fhedt_ready = 0;
        return status;
    }

    sixel_lookup_fhedt_8bit_shared_set_signature(lut->fhedt->shared,
                                                signature);
    lut->fhedt_ready = 1;

    return SIXEL_OK;
}

static int
sixel_lookup_fhedt_parse_flag_8bit(char const *text, int default_value)
{
    long parsed;
    char *endptr;

    if (text == NULL || text[0] == '\0') {
        return default_value;
    }

    errno = 0;
    endptr = NULL;
    parsed = strtol(text, &endptr, 10);
    if (errno == ERANGE || endptr == text || *endptr != '\0') {
        return default_value;
    }

    if (parsed == 0L) {
        return 0;
    }
    if (parsed == 1L) {
        return 1;
    }

    return default_value;
}

static int
sixel_lookup_fhedt_env_resolution_8bit(void)
{
    char const *env;
    long parsed;
    char *endptr;

    env = sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_RESOLUTION");
    if (env == NULL || env[0] == '\0') {
        return 64;
    }

    errno = 0;
    endptr = NULL;
    parsed = strtol(env, &endptr, 10);
    if (errno == ERANGE || endptr == env || *endptr != '\0') {
        return 64;
    }

    if (parsed == 64L || parsed == 128L || parsed == 256L) {
        return (int)parsed;
    }

    return 64;
}

static int
sixel_lookup_fhedt_env_refine_8bit(void)
{
    return sixel_lookup_fhedt_parse_flag_8bit(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_REFINE"),
        1);
}

static int
sixel_lookup_fhedt_env_shared_8bit(void)
{
    return sixel_lookup_fhedt_parse_flag_8bit(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_SHARED"),
        1);
}

static int
sixel_lookup_fhedt_env_use_dist2_8bit(void)
{
    /*
     * Dist2 is disabled by default because measurements have not shown
     * consistent wins.  Enable explicitly when experimenting with boundary
     * refinement short-circuiting.
     */
    return sixel_lookup_fhedt_parse_flag_8bit(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_USE_DIST2"),
        0);
}

static int
sixel_lookup_fhedt_env_use_cache_8bit(void)
{
    /*
     * The cache is disabled by default because its benefit has not been
     * demonstrated.  Callers can opt in for experiments without impacting
     * parallel TLS availability checks.
     */
    return sixel_lookup_fhedt_parse_flag_8bit(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_USE_CACHE"),
        0);
}

static void
sixel_lookup_8bit_release_cache(sixel_lookup_8bit_t *lut)
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
sixel_lookup_8bit_prepare_cache(sixel_lookup_8bit_t *lut)
{
    size_t expected;
    size_t bytes;
    size_t index;

    if (lut == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_lookup_8bit_policy_uses_cache(lut->policy)) {
        return SIXEL_OK;
    }
    if (lut->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /* Allocate a dense RGB quantization table for 5/6bit policies.
     * The packed index matches sixel_lookup_8bit_pack_color() so each slot
     * can store the resolved palette entry or remain empty.
     */
    expected = sixel_lookup_8bit_dense_size((unsigned int)lut->depth,
                                    &lut->quant);
    if (expected == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (expected > SIZE_MAX / sizeof(int32_t)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_8bit_prepare_cache: dense table too large.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (lut->dense != NULL && lut->dense_size != expected) {
        sixel_lookup_8bit_release_cache(lut);
    }

    if (lut->dense == NULL) {
        bytes = expected * sizeof(int32_t);
        lut->dense = (int32_t *)sixel_allocator_malloc(lut->allocator,
                                                       bytes);
        if (lut->dense == NULL) {
            sixel_helper_set_additional_message(
                "sixel_lookup_8bit_prepare_cache: allocation failed.");
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
sixel_lookup_8bit_lookup_fast(sixel_lookup_8bit_t *lut,
                             unsigned char const *pixel)
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
        bucket = sixel_lookup_8bit_pack_color(lut, pixel);
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
        /*
         * Avoid updating the shared dense cache while parallel dither is
         * active to prevent data races between worker threads.
         */
        if (sixel_lookup_parallel_dither_active() == 0) {
            if ((size_t)bucket < lut->dense_size) {
                lut->dense[bucket] = result;
            }
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
    int component;

    status = SIXEL_FALSE;
    if (lut == NULL) {
        goto end;
    }

    lut->lock_ready = 0;
    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        lut->weights[component] = 1;
        lut->weights_sq[component] = 1U;
        memset(lut->scales[component], 0,
               sizeof(lut->scales[component]));
        lut->weight_palette[component] = NULL;
    }
    lut->palette = NULL;
    lut->ncolors = 0;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;

#if SIXEL_ENABLE_THREADS
    status = sixel_mutex_init(&lut->lock);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    lut->lock_ready = 1;
#endif

    /*
     * Single-threaded builds intentionally skip mutex initialization.
     * CERTLUT still works because lock/unlock helpers already guard on
     * lock_ready and become no-ops when synchronization is unnecessary.
     */
    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_certlut_release(sixel_certlut_t *lut)
{
    int component;

    if (lut == NULL) {
        return;
    }
    free(lut->level0);
    free(lut->pool);
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        free(lut->weight_palette[component]);
        lut->weight_palette[component] = NULL;
    }
    free(lut->kdnodes);
    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    if (lut->lock_ready != 0) {
        sixel_mutex_destroy(&lut->lock);
        lut->lock_ready = 0;
    }
}

static int
sixel_certlut_prepare_palette_terms(sixel_certlut_t *lut)
{
    int status;
    size_t count;
    int index;
    int component;
    int32_t *terms[SIXEL_CERTLUT_COMPONENTS];

    status = SIXEL_FALSE;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        terms[component] = NULL;
    }
    if (lut == NULL) {
        goto end;
    }
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    count = (size_t)lut->ncolors;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        terms[component] = (int32_t *)malloc(count * sizeof(int32_t));
        if (terms[component] == NULL) {
            goto end;
        }
    }
    for (index = 0; index < lut->ncolors; ++index) {
        for (component = 0; component < SIXEL_CERTLUT_COMPONENTS;
                ++component) {
            terms[component][index]
                = lut->weights[component]
                * (int)lut->palette[index].comp[component];
        }
    }
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        free(lut->weight_palette[component]);
        lut->weight_palette[component] = terms[component];
        terms[component] = NULL;
    }
    status = SIXEL_OK;

end:
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        free(terms[component]);
    }
    return status;
}

static void
sixel_certlut_cell_center(int comp1_min,
                          int comp2_min,
                          int comp3_min,
                          int size,
                          int *comp1_center,
                          int *comp2_center,
                          int *comp3_center)
{
    int half;

    half = size / 2;
    *comp1_center = comp1_min + half;
    *comp2_center = comp2_min + half;
    *comp3_center = comp3_min + half;
    if (size == 1) {
        *comp1_center = comp1_min;
        *comp2_center = comp2_min;
        *comp3_center = comp3_min;
    }
}

static void
sixel_certlut_weight_init(sixel_certlut_t *lut,
                          int comp1_weight,
                          int comp2_weight,
                          int comp3_weight)
{
    int component;
    int i;
    int input[SIXEL_CERTLUT_COMPONENTS];

    input[0] = comp1_weight;
    input[1] = comp2_weight;
    input[2] = comp3_weight;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        lut->weights[component] = input[component];
        lut->weights_sq[component]
            = (uint64_t)input[component] * (uint64_t)input[component];
        for (i = 0; i < 256; ++i) {
            lut->scales[component][i] = input[component] * i;
        }
    }
}

static uint64_t
sixel_certlut_distance_precomputed(sixel_certlut_t const *lut,
                                   int index,
                                   int32_t const scaled_components[])
{
    uint64_t distance;
    int64_t diff;
    int component;

    distance = 0U;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        diff = (int64_t)scaled_components[component]
             - (int64_t)lut->weight_palette[component][index];
        distance += (uint64_t)(diff * diff);
    }

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
    int64_t delta;
    int component;

    if (best_idx < 0 || second_idx < 0) {
        return 1;
    }

    /*
     * The certification bound compares the squared distance gap against the
     * palette separation scaled by the cube diameter.  If the gap wins the
     * entire cube maps to the current best palette entry.
     */
    delta_sq = second_dist - best_dist;
    weight_term = 0U;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        delta = (int64_t)lut->weight_palette[component][second_idx]
              - (int64_t)lut->weight_palette[component][best_idx];
        weight_term += (uint64_t)(delta * delta);
    }
    rhs = (uint64_t)SIXEL_CERTLUT_COMPONENTS
        * (uint64_t)size * (uint64_t)size * weight_term;

    return delta_sq * delta_sq > rhs;
}

static void
sixel_certlut_lock(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    if (lut->lock_ready != 0) {
        sixel_mutex_lock(&lut->lock);
    }
}

static void
sixel_certlut_unlock(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    if (lut->lock_ready != 0) {
        sixel_mutex_unlock(&lut->lock);
    }
}

static void
sixel_certlut_disable_locking(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    if (lut->lock_ready == 0) {
        return;
    }

    /*
     * Thread-local CERTLUT instances never contend for the shared pool, so
     * the mutex only adds overhead.  Tear it down to skip lock/unlock in the
     * lookup hot path.
     */
    sixel_mutex_destroy(&lut->lock);
    lut->lock_ready = 0;
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
    if (axis < 0) {
        axis = 0;
    } else if (axis >= SIXEL_CERTLUT_COMPONENTS) {
        axis = SIXEL_CERTLUT_COMPONENTS - 1;
    }

    return (int)color->comp[axis];
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

    axis = depth % SIXEL_CERTLUT_COMPONENTS;
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
    if (axis < 0) {
        axis = 0;
    } else if (axis >= SIXEL_CERTLUT_COMPONENTS) {
        axis = SIXEL_CERTLUT_COMPONENTS - 1;
    }
    weight = lut->weights_sq[axis];

    return weight * abs_diff * abs_diff;
}

static void
sixel_certlut_consider_candidate(sixel_certlut_t const *lut,
                                 int candidate,
                                 int32_t const scaled_components[],
                                 int *best_idx,
                                 uint64_t *best_dist,
                                 int *second_idx,
                                 uint64_t *second_dist)
{
    uint64_t distance;

    distance = sixel_certlut_distance_precomputed(lut,
                                                  candidate,
                                                  scaled_components);
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
                            int const components[],
                            int32_t const scaled_components[],
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
    int component_value;

    if (node_index < 0) {
        return;
    }
    node = &lut->kdnodes[node_index];
    sixel_certlut_consider_candidate(lut,
                                     node->index,
                                     scaled_components,
                                     best_idx,
                                     best_dist,
                                     second_idx,
                                     second_dist);

    axis = (int)node->axis;
    value = sixel_certlut_palette_component(lut, node->index, axis);
    component_value = components[axis];
    diff = component_value - value;
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
                                    components,
                                    scaled_components,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
    axis_bound = sixel_certlut_axis_distance(lut, diff, axis);
    if (far_child >= 0 && axis_bound <= *second_dist) {
        sixel_certlut_kdtree_search(lut,
                                    far_child,
                                    components,
                                    scaled_components,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
}

static void
sixel_certlut_distance_pair(sixel_certlut_t const *lut,
                            int comp1,
                            int comp2,
                            int comp3,
                            int *best_idx,
                            int *second_idx,
                            uint64_t *best_dist,
                            uint64_t *second_dist)
{
    int i;
    int best_candidate;
    int second_candidate;
    uint64_t best_value;
    uint64_t second_value;
    uint64_t distance;
    int components[SIXEL_CERTLUT_COMPONENTS];
    int clamped[SIXEL_CERTLUT_COMPONENTS];
    int component;
    int32_t scaled[SIXEL_CERTLUT_COMPONENTS];

    best_candidate = (-1);
    second_candidate = (-1);
    best_value = UINT64_MAX;
    second_value = UINT64_MAX;
    components[0] = comp1;
    components[1] = comp2;
    components[2] = comp3;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        clamped[component] = components[component];
        if (clamped[component] < 0) {
            clamped[component] = 0;
        } else if (clamped[component] > 255) {
            clamped[component] = 255;
        }
        scaled[component]
            = lut->scales[component][clamped[component]];
    }
    if (lut->kdnodes != NULL && lut->kdtree_root >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    lut->kdtree_root,
                                    components,
                                    scaled,
                                    &best_candidate,
                                    &best_value,
                                    &second_candidate,
                                    &second_value);
    } else {
        for (i = 0; i < lut->ncolors; ++i) {
            distance = sixel_certlut_distance_precomputed(lut,
                                                          i,
                                                          scaled);
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
sixel_certlut_fallback(sixel_certlut_t const *lut,
                       int comp1,
                       int comp2,
                       int comp3)
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
                                comp1,
                                comp2,
                                comp3,
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
                         int comp1_min,
                         int comp2_min,
                         int comp3_min,
                         int size)
{
    SIXELSTATUS status;
    int center1;
    int center2;
    int center3;
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
                "build_cell comp1=%d comp2=%d comp3=%d size=%d\n",
                comp1_min,
                comp2_min,
                comp3_min,
                size);
#endif
    }
    if (*cell != 0U) {
        status = SIXEL_OK;
        goto end;
    }

    /*
     * Each node represents an axis-aligned cube in component space.  The
     * builder certifies the dominant palette index by checking the distance
     * gap at the cell center.  When certification fails the cube is split into
     * eight octants backed by a pool block.  Children remain unbuilt until
     * lookups descend into them, keeping the workload proportional to actual
     * queries.
     */
    status = SIXEL_FALSE;
    sixel_certlut_cell_center(comp1_min,
                              comp2_min,
                              comp3_min,
                              size,
                              &center1,
                              &center2,
                              &center3);
    sixel_certlut_distance_pair(lut,
                                center1,
                                center2,
                                center3,
                                &best_idx,
                                &second_idx,
                                &best_dist,
                                &second_dist);
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
    if (sixel_certlut_is_cell_safe(lut,
                                   best_idx,
                                   second_idx,
                                   size,
                                   best_dist,
                                   second_dist)) {
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
        fprintf(stderr, "  branch offset=%u\n", offset);
#endif
    status = SIXEL_OK;

end:
    return status;
}

SIXELSTATUS
sixel_certlut_build(sixel_certlut_t *lut,
                    sixel_certlut_color_t const *palette,
                    int ncolors,
                    int wcomp1,
                    int wcomp2,
                    int wcomp3)
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
    sixel_certlut_weight_init(lut, wcomp1, wcomp2, wcomp3);
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
sixel_certlut_lookup(sixel_certlut_t *lut,
                     uint8_t comp1,
                     uint8_t comp2,
                     uint8_t comp3)
{
    uint8_t result;
    uint32_t entry;
    uint32_t offset;
    uint32_t index;
    uint32_t *children;
    uint32_t *cell;
    int shift;
    int child;
    int status;
    int size;
    int comp1_min;
    int comp2_min;
    int comp3_min;
    int step;
    int locked;

    result = 0U;
    locked = 0;
    if (lut == NULL || lut->level0 == NULL) {
        return 0U;
    }
    /*
     * Lazy cell materialization reallocates the shared pool.  Serialize the
     * lookup so realloc cannot race with concurrent threads traversing or
     * expanding neighbouring cells.
     */
    sixel_certlut_lock(lut);
    locked = 1;
    /*
     * Cells are created lazily.  A zero entry indicates an uninitialized
     * subtree, so the builder is invoked with the cube bounds of the current
     * traversal.  Should allocation fail we fall back to a direct brute-force
     * palette search for the queried pixel.
     */
    index = ((uint32_t)(comp1 >> 2) << 12)
          | ((uint32_t)(comp2 >> 2) << 6)
          | (uint32_t)(comp3 >> 2);
    cell = lut->level0 + index;
    size = 4;
    comp1_min = (int)(comp1 & 0xfc);
    comp2_min = (int)(comp2 & 0xfc);
    comp3_min = (int)(comp3 & 0xfc);
    entry = *cell;
    if (entry == 0U) {
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "lookup build level0 comp1=%u comp2=%u comp3=%u\n",
                (unsigned int)comp1,
                (unsigned int)comp2,
                (unsigned int)comp3);
#endif
        status = sixel_certlut_build_cell(lut,
                                          cell,
                                          comp1_min,
                                          comp2_min,
                                          comp3_min,
                                          size);
        if (SIXEL_FAILED(status)) {
            result = sixel_certlut_fallback(lut,
                                            (int)comp1,
                                            (int)comp2,
                                            (int)comp3);
            goto end;
        }
        entry = *cell;
    }
    shift = 1;
    while ((entry & 0x80000000U) == 0U) {
        offset = entry & 0x3fffffffU;
        children = (uint32_t *)(void *)(lut->pool + offset);
        child = (((int)(comp1 >> shift) & 1) << 2)
              | (((int)(comp2 >> shift) & 1) << 1)
              | ((int)(comp3 >> shift) & 1);
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
        comp1_min += step * ((child >> 2) & 1);
        comp2_min += step * ((child >> 1) & 1);
        comp3_min += step * (child & 1);
        size = step;
        cell = children + (size_t)child;
        entry = *cell;
        if (entry == 0U) {
#ifdef DEBUG_LUT_TRACE
            fprintf(stderr,
                    "lookup build child size=%d comp1=%d comp2=%d comp3=%d\n",
                    size,
                    comp1_min,
                    comp2_min,
                    comp3_min);
#endif
            status = sixel_certlut_build_cell(lut,
                                              cell,
                                              comp1_min,
                                              comp2_min,
                                              comp3_min,
                                              size);
            if (SIXEL_FAILED(status)) {
                result = sixel_certlut_fallback(lut,
                                                (int)comp1,
                                                (int)comp2,
                                                (int)comp3);
                goto end;
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

    result = (uint8_t)(entry & 0xffU);

end:
    if (locked != 0) {
        sixel_certlut_unlock(lut);
    }

    return result;
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

void
sixel_lookup_8bit_init(sixel_lookup_8bit_t *lut, sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(sixel_lookup_8bit_t));
    lut->allocator = allocator;
    lut->policy = sixel_lookup_8bit_policy_normalize(SIXEL_LUT_POLICY_6BIT);
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
    lut->packing = SIXEL_LOOKUP_PACK_LINEAR;
    lut->palette = NULL;
    lut->quant.channel_shift = 0U;
    lut->quant.channel_bits = 8U;
    lut->quant.channel_mask = 0xffU;
    lut->dense = NULL;
    lut->dense_size = 0U;
    lut->dense_ready = 0;
    lut->cert_ready = 0;
    lut->fhedt = NULL;
    lut->fhedt_ready = 0;
    lut->vptree = NULL;
    lut->vptree_ready = 0;
    lut->eytz.count = 0;
    lut->eytz.ready = 0;
    lut->eytz.keys = NULL;
    lut->eytz.palette_index = NULL;
    lut->eytz.rank = NULL;
    lut->eytz.sorted_palette_index = NULL;
    lut->eytz.sorted_keys = NULL;
    lut->eytz.window = SIXEL_LOOKUP_EYTZINGER_WINDOW;
    lut->eytz.weights[0] = 1;
    lut->eytz.weights[1] = 1;
    lut->eytz.weights[2] = 1;
    lut->cert = (sixel_certlut_t *)malloc(sizeof(sixel_certlut_t));
    if (lut->cert != NULL) {
        sixel_certlut_init(lut->cert);
    }
    (void)sixel_lookup_fhedt_8bit_create(allocator, &lut->fhedt);
    (void)sixel_lookup_vptree_8bit_create(allocator, &lut->vptree);
}

SIXELSTATUS
sixel_lookup_8bit_configure(sixel_lookup_8bit_t *lut,
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
    int normalized;

    (void)pixelformat;

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
    lut->packing = sixel_lookup_8bit_env_packing();
    normalized = sixel_lookup_8bit_policy_normalize(policy);
    lut->policy = normalized;
    lut->quant = sixel_lookup_8bit_quant_make((unsigned int)depth, normalized);
    lut->dense_ready = 0;

    /*
     * Reconfiguration may switch policies or rebuild CERTLUT with a different
     * palette.  Release previous CERTLUT allocations unconditionally so stale
     * data cannot survive when cert_ready is reset for the new configuration.
     */
    if (lut->cert != NULL) {
        sixel_certlut_free(lut->cert);
    }
    lut->cert_ready = 0;
    lut->eytz.ready = 0;
    lut->vptree_ready = 0;

    if (normalized == SIXEL_LUT_POLICY_FHEDT) {
        status = sixel_lookup_8bit_configure_fhedt(lut,
                                                  palette,
                                                  ncolors,
                                                  complexion,
                                                  wcomp1,
                                                  wcomp2,
                                                  wcomp3,
                                                  pixelformat);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_8bit_configure: FHEDT failed; "
                "falling back to CERTLUT.");
            normalized = SIXEL_LUT_POLICY_CERTLUT;
        } else {
            return SIXEL_OK;
        }
    } else if (normalized == SIXEL_LUT_POLICY_VPTREE) {
        if (lut->vptree == NULL) {
            status = sixel_lookup_vptree_8bit_create(lut->allocator,
                                                     &lut->vptree);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_lookup_8bit_configure: VP-tree allocation failed.");
                normalized = SIXEL_LUT_POLICY_CERTLUT;
            }
        }
        if (normalized == SIXEL_LUT_POLICY_VPTREE) {
            status = sixel_lookup_vptree_8bit_configure(lut->vptree,
                                                        palette,
                                                        ncolors,
                                                        depth,
                                                        complexion);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_lookup_8bit_configure: VP-tree failed; "
                    "falling back to CERTLUT.");
                normalized = SIXEL_LUT_POLICY_CERTLUT;
            } else {
                lut->vptree_ready = 1;
                return SIXEL_OK;
            }
        }
    } else if (normalized == SIXEL_LUT_POLICY_EYTZINGER) {
        status = sixel_lookup_8bit_configure_1d_eytzinger(lut,
                                                       palette,
                                                       ncolors,
                                                       wcomp1,
                                                       wcomp2,
                                                       wcomp3);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_8bit_configure: Eytzinger failed; "
                "falling back to CERTLUT.");
            normalized = SIXEL_LUT_POLICY_CERTLUT;
        } else {
            lut->policy = normalized;
            return SIXEL_OK;
        }
    } else {
        lut->fhedt_ready = 0;
        sixel_lookup_8bit_1d_eytzinger_release(lut);
    }

    lut->policy = normalized;

    if (sixel_lookup_8bit_policy_uses_cache(normalized)) {
        if (depth != 3) {
            sixel_helper_set_additional_message(
                "sixel_lookup_8bit_configure: fast LUT requires RGB pixels.");
            return SIXEL_BAD_ARGUMENT;
        }
        status = sixel_lookup_8bit_prepare_cache(lut);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    } else {
        sixel_lookup_8bit_release_cache(lut);
        status = SIXEL_OK;
    }

    if (normalized == SIXEL_LUT_POLICY_CERTLUT) {
        if (lut->cert == NULL) {
            sixel_helper_set_additional_message(
                "sixel_lookup_8bit_configure: cert buffer missing.");
            return SIXEL_BAD_ALLOCATION;
        }
        if (sixel_lookup_env_shared_certlut() == 0) {
            sixel_certlut_disable_locking(lut->cert);
        }
        status = sixel_certlut_build(lut->cert,
                                     (sixel_certlut_color_t const *)palette,
                                     ncolors,
                                     wcomp1,
                                     wcomp2,
                                     wcomp3);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        lut->cert_ready = 1;
    }

    return SIXEL_OK;
}

int
sixel_lookup_8bit_map_pixel(sixel_lookup_8bit_t *lut,
                            unsigned char const *pixel)
{
    if (lut == NULL || pixel == NULL) {
        return 0;
    }
    if (lut->policy == SIXEL_LUT_POLICY_FHEDT) {
        if (lut->fhedt_ready && lut->fhedt != NULL) {
            return sixel_lookup_fhedt_8bit_map(lut->fhedt, pixel);
        }
        return 0;
    }
    if (lut->policy == SIXEL_LUT_POLICY_VPTREE) {
        if (lut->vptree_ready && lut->vptree != NULL) {
            return sixel_lookup_vptree_8bit_map(lut->vptree, pixel);
        }
        return 0;
    }
    if (lut->policy == SIXEL_LUT_POLICY_EYTZINGER) {
        return sixel_lookup_8bit_lookup_1d_eytzinger(lut, pixel);
    }
    if (lut->policy == SIXEL_LUT_POLICY_CERTLUT) {
        if (!lut->cert_ready) {
            return 0;
        }
        return (int)sixel_certlut_lookup(lut->cert,
                                         pixel[0],
                                         pixel[1],
                                         pixel[2]);
    }

    return sixel_lookup_8bit_lookup_fast(lut, pixel);
}

void
sixel_lookup_8bit_clear(sixel_lookup_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_8bit_release_cache(lut);
    if (lut->cert != NULL) {
        sixel_certlut_free(lut->cert);
    }
    lut->cert_ready = 0;
    if (lut->fhedt != NULL) {
        sixel_lookup_fhedt_8bit_unref(lut->fhedt);
        lut->fhedt = NULL;
    }
    lut->fhedt_ready = 0;
    if (lut->vptree != NULL) {
        sixel_lookup_vptree_8bit_unref(lut->vptree);
        lut->vptree = NULL;
    }
    lut->vptree_ready = 0;
    sixel_lookup_8bit_1d_eytzinger_release(lut);
    lut->palette = NULL;
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
}

void
sixel_lookup_8bit_finalize(sixel_lookup_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_8bit_clear(lut);
    if (lut->cert != NULL) {
        sixel_certlut_free(lut->cert);
        free(lut->cert);
        lut->cert = NULL;
    }
    lut->allocator = NULL;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
