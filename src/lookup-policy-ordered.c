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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lookup-policy-ordered.h"
#include "lookup-policy-blockcull.h"
#include "sixel_atomic.h"

#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
#include <arm_neon.h>
#endif


/*
 * IDL (internal contract)
 *
 * class LookupOrdered : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   map_pixels(pixels);
 * }
 *
 * Byte queries start from an 8x8x8 seed grid and scan a palette row
 * sorted by distance from that seed. The triangle inequality excludes
 * colors farther than twice the query-to-seed distance. Both bounds and
 * final squared distances are exact integers; ties keep the first index.
 * Float queries retain the exact blockcull backend and native coordinates.
 */

enum {
    SIXEL_LOOKUP_POLICY_ORDERED_COMPONENTS = 3
};

typedef struct sixel_lookup_policy_ordered_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int prepared;
    int ncolors;
    unsigned char palette[SIXEL_PALETTE_MAX * 3];
    /* Rows omit their center and repeat the last entry for SIMD tails.
     * Packed keys contain squared distance followed by original index;
     * packed colors contain RGB followed by original index. */
    uint32_t keys[SIXEL_PALETTE_MAX][SIXEL_PALETTE_MAX];
    uint32_t colors[SIXEL_PALETTE_MAX][SIXEL_PALETTE_MAX];
    unsigned char seeds[512];
} sixel_lookup_policy_ordered_object_t;

static sixel_lookup_policy_ordered_object_t *
sixel_lookup_policy_ordered_from_base(
    sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_ordered_object_t *)(void *)policy;
}

static sixel_lookup_policy_ordered_object_t const *
sixel_lookup_policy_ordered_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_ordered_object_t const *)(void const *)
        policy;
}

static void
sixel_lookup_policy_ordered_reset(
    sixel_lookup_policy_ordered_object_t *object)
{
    object->prepared = 0;
    object->ncolors = 0;
}

static void
sixel_lookup_policy_ordered_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_ordered_object_t *object;

    if (policy == NULL) {
        return;
    }
    object = sixel_lookup_policy_ordered_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_ordered_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_ordered_object_t *object;
    sixel_allocator_t *allocator;

    if (policy == NULL) {
        return;
    }
    object = sixel_lookup_policy_ordered_from_base(policy);
    if (sixel_atomic_fetch_sub_u32(&object->ref, 1U) == 1U) {
        sixel_lookup_policy_ordered_reset(object);
        allocator = object->allocator;
        object->allocator = NULL;
        if (allocator != NULL) {
            sixel_allocator_free(allocator, object);
            sixel_allocator_unref(allocator);
        }
    }
}

/* Preparation copies the palette, so reuse hints are not consumed. */
static SIXELSTATUS
sixel_lookup_policy_ordered_check_request(
    sixel_lookup_policy_prepare_request_t const *request)
{
    if (request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->depth != SIXEL_LOOKUP_POLICY_ORDERED_COMPONENTS) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->reqcolor > SIXEL_PALETTE_MAX) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_ordered: palette exceeds 256 colors.");
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

/* Byte squared distances fit in 18 bits; packed keys fit in 26 bits. */
static uint32_t
sixel_lookup_policy_ordered_distance(
    unsigned char const *a, unsigned char const *b)
{
    int r, g, blue;

    r = (int)a[0] - (int)b[0];
    g = (int)a[1] - (int)b[1];
    blue = (int)a[2] - (int)b[2];
    return (uint32_t)(r * r + g * g + blue * blue);
}

/* Input indexes already ascend. Three stable radix passes on the distance
 * retain that tie order and avoid comparator calls for every palette row. */
static void
sixel_lookup_policy_ordered_sort(uint32_t *keys, int count)
{
    uint32_t scratch[SIXEL_PALETTE_MAX];
    unsigned int offsets[256];
    unsigned int shift, bins, bin, total, next;
    int i;

    for (shift = 8; shift <= 24; shift += 8) {
        bins = shift == 24 ? 4U : 256U;
        memset(offsets, 0, bins * sizeof(offsets[0]));
        for (i = 0; i < count; ++i) {
            ++offsets[(keys[i] >> shift) & 255U];
        }
        total = 0U;
        for (bin = 0; bin < bins; ++bin) {
            next = total + offsets[bin];
            offsets[bin] = total;
            total = next;
        }
        for (i = 0; i < count; ++i) {
            bin = (keys[i] >> shift) & 255U;
            scratch[offsets[bin]++] = keys[i];
        }
        memcpy(keys, scratch, (size_t)count * sizeof(keys[0]));
    }
}

static SIXELSTATUS
sixel_lookup_policy_ordered_prepare_8bit(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_ordered_object_t *object;
    SIXELSTATUS status;
    uint32_t keys[SIXEL_PALETTE_MAX];
    uint32_t key, packed;
    int row, i, index, count, cell, q[3], r, g, b, distance, minimum;

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    object = sixel_lookup_policy_ordered_from_base(policy);
    status = sixel_lookup_policy_ordered_check_request(request);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_ordered_reset(object);
        return status;
    }
    /* Keep an owned, immutable table; hints never substitute for identity. */
    if (object->prepared && object->ncolors == request->reqcolor
            && memcmp(object->palette, request->palette,
                      (size_t)request->reqcolor * 3U) == 0) {
        return SIXEL_OK;
    }
    sixel_lookup_policy_ordered_reset(object);
    memcpy(object->palette, request->palette,
           (size_t)request->reqcolor * 3U);
    object->ncolors = request->reqcolor;
    for (row = 0; row < object->ncolors; ++row) {
        for (i = 0; i < object->ncolors; ++i) {
            keys[i] = (sixel_lookup_policy_ordered_distance(
                object->palette + row * 3, object->palette + i * 3) << 8)
                | (uint32_t)i;
        }
        sixel_lookup_policy_ordered_sort(keys, object->ncolors);
        count = 0;
        for (i = 0; i < object->ncolors; ++i) {
            key = keys[i];
            index = (int)(key & 255U);
            if (index == row) {
                continue;
            }
            packed = (uint32_t)object->palette[index * 3]
                | ((uint32_t)object->palette[index * 3 + 1] << 8)
                | ((uint32_t)object->palette[index * 3 + 2] << 16)
                | ((uint32_t)index << 24);
            object->keys[row][count] = key;
            object->colors[row][count++] = packed;
        }
        while ((count & 3) != 0) {
            object->keys[row][count] = object->keys[row][count - 1];
            object->colors[row][count] = object->colors[row][count - 1];
            ++count;
        }
    }
    /* Rational cell centers scaled by sixteen avoid seed rounding bias.
     * Seeds are only starting colors; query coordinates remain unchanged. */
    for (cell = 0; cell < 512; ++cell) {
        q[0] = (2 * (cell >> 6) + 1) * 255;
        q[1] = (2 * ((cell >> 3) & 7) + 1) * 255;
        q[2] = (2 * (cell & 7) + 1) * 255;
        minimum = INT_MAX;
        index = 0;
        for (i = 0; i < object->ncolors; ++i) {
            r = q[0] - (int)object->palette[i * 3] * 16;
            g = q[1] - (int)object->palette[i * 3 + 1] * 16;
            b = q[2] - (int)object->palette[i * 3 + 2] * 16;
            distance = r * r + g * g + b * b;
            if (distance < minimum) {
                minimum = distance;
                index = i;
            }
        }
        object->seeds[cell] = (unsigned char)index;
    }
    object->prepared = 1;
    return SIXEL_OK;
}

static int
sixel_lookup_policy_ordered_map_pixel_hint_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel, int hint)
{
    sixel_lookup_policy_ordered_object_t const *object;
    uint32_t best, key, limit;
    int cell, center, at;
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
    uint32x4_t packed, scores, mask;
    int32x4_t dr, dg, db, distance;
#else
    uint32_t color;
    int lane, r, g, b;
#endif

    if (policy == NULL || pixel == NULL) {
        return 0;
    }
    object = sixel_lookup_policy_ordered_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }
    cell = ((int)(pixel[0] >> 5) << 6)
        | ((int)(pixel[1] >> 5) << 3) | (pixel[2] >> 5);
    center = object->seeds[cell];
    best = (sixel_lookup_policy_ordered_distance(
        pixel, object->palette + center * 3) << 8) | (uint32_t)center;
    if (hint >= 0 && hint < object->ncolors) {
        key = (sixel_lookup_policy_ordered_distance(
            pixel, object->palette + hint * 3) << 8) | (uint32_t)hint;
        if (key < best) {
            best = key;
            center = hint;
        }
    }
    /* If c is our seed and p can win or tie, |p-c| <= 2|q-c|.
     * Strict exclusion keeps boundary ties; the packed key resolves them
     * by original index even though the row is not in palette order. */
    limit = 4U * (best >> 8);
    for (at = 0; at < object->ncolors - 1; at += 4) {
        if ((object->keys[center][at] >> 8) > limit) {
            break;
        }
#if HAVE_NEON && HAVE_ARM_NEON_H && defined(__aarch64__)
        packed = vld1q_u32(object->colors[center] + at);
        mask = vdupq_n_u32(255U);
        dr = vsubq_s32(vreinterpretq_s32_u32(vandq_u32(packed, mask)),
                      vdupq_n_s32(pixel[0]));
        dg = vsubq_s32(vreinterpretq_s32_u32(
            vandq_u32(vshrq_n_u32(packed, 8), mask)),
            vdupq_n_s32(pixel[1]));
        db = vsubq_s32(vreinterpretq_s32_u32(
            vandq_u32(vshrq_n_u32(packed, 16), mask)),
            vdupq_n_s32(pixel[2]));
        distance = vmlaq_s32(vmulq_s32(dr, dr), dg, dg);
        distance = vmlaq_s32(distance, db, db);
        scores = vorrq_u32(vshlq_n_u32(
            vreinterpretq_u32_s32(distance), 8), vshrq_n_u32(packed, 24));
        key = vminvq_u32(scores);
        if (key < best) {
            best = key;
        }
#else
        for (lane = 0; lane < 4; ++lane) {
            color = object->colors[center][at + lane];
            r = (int)(color & 255U) - (int)pixel[0];
            g = (int)((color >> 8) & 255U) - (int)pixel[1];
            b = (int)((color >> 16) & 255U) - (int)pixel[2];
            key = ((uint32_t)(r * r + g * g + b * b) << 8)
                | (color >> 24);
            if (key < best) {
                best = key;
            }
        }
#endif
    }
    return (int)(best & 255U);
}

static int
sixel_lookup_policy_ordered_map_pixel_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    return sixel_lookup_policy_ordered_map_pixel_hint_8bit(policy, pixel, -1);
}

static int
sixel_lookup_policy_ordered_get_kind(
    sixel_lookup_policy_interface_t const *policy)
{
    (void)policy;
    return SIXEL_LUT_POLICY_ORDERED;
}

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_ordered_8bit_vtbl = {
    sixel_lookup_policy_ordered_ref,
    sixel_lookup_policy_ordered_unref,
    sixel_lookup_policy_ordered_prepare_8bit,
    sixel_lookup_policy_ordered_map_pixel_8bit,
    sixel_lookup_policy_ordered_map_pixel_hint_8bit,
    sixel_lookup_policy_map_pixels_default,
    NULL,
    sixel_lookup_policy_ordered_get_kind,
};

static SIXELSTATUS
sixel_lookup_policy_ordered_new(
    sixel_allocator_t *allocator,
    sixel_lookup_policy_vtbl_t *vtbl,
    void **policy)
{
    sixel_lookup_policy_ordered_object_t *object;

    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;
    object = (sixel_lookup_policy_ordered_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_ordered_new: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memset(object, 0, sizeof(*object));
    object->base.vtbl = vtbl;
    object->ref = 1U;
    object->allocator = allocator;
    sixel_allocator_ref(allocator);
    *policy = &object->base;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_lookup_policy_ordered_8bit_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    return sixel_lookup_policy_ordered_new(
        allocator, &g_sixel_lookup_policy_ordered_8bit_vtbl, policy);
}

SIXELSTATUS
sixel_lookup_policy_ordered_float32_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    /* Byte sorted rows cannot bound arbitrary float working coordinates.
     * Reuse the established exact backend without rounding queries or
     * allocating byte rows. */
    return sixel_lookup_policy_blockcull_float32_new(allocator, policy);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
