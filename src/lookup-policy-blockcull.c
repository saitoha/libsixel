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
#include <stdlib.h>
#include <string.h>

#include "lookup-policy-blockcull.h"
#include "lookup-blockcull-float32.h"
#include "pixelformat.h"
#include "sixel_atomic.h"


/*
 * IDL (internal contract)
 *
 * class LookupBlockcull : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   map_pixels(pixels);
 * }
 *
 * A single lookup scans the palette. A run of samples first culls colors
 * that cannot be nearest anywhere in the run's bounding box; see
 * lookup-blockcull-float32.c for the bound. Byte distances are exact
 * integers, so the byte test needs no slack.
 */

enum {
    SIXEL_LOOKUP_POLICY_BLOCKCULL_RUN = 32,
    SIXEL_LOOKUP_POLICY_BLOCKCULL_COMPONENTS = 3
};

typedef struct sixel_lookup_policy_blockcull_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int prepared;
    int ncolors;
    /* Byte class: palette channels as separate integer arrays. */
    int red[SIXEL_PALETTE_MAX];
    int green[SIXEL_PALETTE_MAX];
    int blue[SIXEL_PALETTE_MAX];
    /* Float class. */
    sixel_lookup_blockcull_float32_t *float_lookup;
} sixel_lookup_policy_blockcull_object_t;

static sixel_lookup_policy_blockcull_object_t *
sixel_lookup_policy_blockcull_from_base(
    sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_blockcull_object_t *)(void *)policy;
}

static sixel_lookup_policy_blockcull_object_t const *
sixel_lookup_policy_blockcull_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_blockcull_object_t const *)(void const *)
        policy;
}

static void
sixel_lookup_policy_blockcull_reset(
    sixel_lookup_policy_blockcull_object_t *object)
{
    sixel_lookup_blockcull_float32_destroy(object->float_lookup);
    object->float_lookup = NULL;
    object->prepared = 0;
    object->ncolors = 0;
}

static void
sixel_lookup_policy_blockcull_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_blockcull_object_t *object;

    if (policy == NULL) {
        return;
    }
    object = sixel_lookup_policy_blockcull_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_blockcull_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_blockcull_object_t *object;
    sixel_allocator_t *allocator;

    if (policy == NULL) {
        return;
    }
    object = sixel_lookup_policy_blockcull_from_base(policy);
    if (sixel_atomic_fetch_sub_u32(&object->ref, 1U) == 1U) {
        sixel_lookup_policy_blockcull_reset(object);
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
sixel_lookup_policy_blockcull_check_request(
    sixel_lookup_policy_prepare_request_t const *request)
{
    if (request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->depth != SIXEL_LOOKUP_POLICY_BLOCKCULL_COMPONENTS) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->reqcolor > SIXEL_PALETTE_MAX) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_blockcull: palette exceeds 256 colors.");
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_blockcull_prepare_8bit(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_blockcull_object_t *object;
    SIXELSTATUS status;
    int i;

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    object = sixel_lookup_policy_blockcull_from_base(policy);
    sixel_lookup_policy_blockcull_reset(object);
    status = sixel_lookup_policy_blockcull_check_request(request);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    for (i = 0; i < request->reqcolor; ++i) {
        object->red[i] = request->palette[i * 3 + 0];
        object->green[i] = request->palette[i * 3 + 1];
        object->blue[i] = request->palette[i * 3 + 2];
    }
    object->ncolors = request->reqcolor;
    object->prepared = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_blockcull_prepare_float32(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_blockcull_object_t *object;
    SIXELSTATUS status;
    float palette[SIXEL_PALETTE_MAX * 3];
    float weights[SIXEL_LOOKUP_POLICY_BLOCKCULL_COMPONENTS];
    float range;
    int stride;
    int i;
    int c;

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    object = sixel_lookup_policy_blockcull_from_base(policy);
    sixel_lookup_policy_blockcull_reset(object);
    status = sixel_lookup_policy_blockcull_check_request(request);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    /* The VP-tree float metric: each channel normalized by its range. */
    for (c = 0; c < SIXEL_LOOKUP_POLICY_BLOCKCULL_COMPONENTS; ++c) {
        range = sixel_pixelformat_float_channel_range(request->pixelformat,
                                                      c);
        if (range <= 0.0f) {
            range = 1.0f;
        }
        weights[c] = (1.0f / range) * (1.0f / range);
    }

    stride = 0;
    if (request->palette_float != NULL && request->float_depth > 0) {
        if (request->float_depth < 3 * (int)sizeof(float)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_blockcull: float palette depth "
                "mismatch.");
            return SIXEL_BAD_ARGUMENT;
        }
        stride = request->float_depth / (int)sizeof(float);
    }
    for (i = 0; i < request->reqcolor; ++i) {
        for (c = 0; c < 3; ++c) {
            palette[i * 3 + c] = stride > 0
                ? request->palette_float[i * stride + c]
                : sixel_pixelformat_byte_to_float(request->pixelformat, c,
                      request->palette[i * 3 + c]);
        }
    }

    status = sixel_lookup_blockcull_float32_create(request->allocator,
                                                   palette,
                                                   request->reqcolor,
                                                   weights,
                                                   &object->float_lookup);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_blockcull: float lookup allocation failed.");
        return status;
    }
    object->ncolors = request->reqcolor;
    object->prepared = 1;
    return SIXEL_OK;
}

/* Strict first minimum over the listed colors, or over the whole palette
 * when candidates is NULL: equal distances keep the lowest index. */
static int
sixel_lookup_policy_blockcull_scan_8bit(
    sixel_lookup_policy_blockcull_object_t const *object,
    int r, int g, int b, int const *candidates, int ncandidates)
{
    int best;
    int minimum;
    int distance;
    int dr;
    int dg;
    int db;
    int i;
    int index;

    best = 0;
    minimum = INT_MAX;
    for (i = 0; i < ncandidates; ++i) {
        index = candidates != NULL ? candidates[i] : i;
        dr = r - object->red[index];
        dg = g - object->green[index];
        db = b - object->blue[index];
        distance = dr * dr + dg * dg + db * db;
        if (distance < minimum) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}

static int
sixel_lookup_policy_blockcull_map_pixel_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_blockcull_object_t const *object;

    if (policy == NULL || pixel == NULL) {
        return 0;
    }
    object = sixel_lookup_policy_blockcull_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }
    return sixel_lookup_policy_blockcull_scan_8bit(
        object, pixel[0], pixel[1], pixel[2], NULL, object->ncolors);
}

/* A single sample has no run to cull over; the hint cannot help. */
static int
sixel_lookup_policy_blockcull_map_pixel_hint_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel,
    int hint)
{
    (void)hint;
    return sixel_lookup_policy_blockcull_map_pixel_8bit(policy, pixel);
}

/* Keep color i when its box distance does not exceed the smallest
 * farthest-corner distance over all colors. Integer arithmetic is exact. */
static int
sixel_lookup_policy_blockcull_candidates_8bit(
    sixel_lookup_policy_blockcull_object_t const *object,
    int const low[3], int const high[3], int *candidates)
{
    int bound;
    int far;
    int near;
    int a;
    int b;
    int gr;
    int gg;
    int gb;
    int count;
    int i;

    bound = INT_MAX;
    for (i = 0; i < object->ncolors; ++i) {
        a = object->red[i] - low[0];
        b = high[0] - object->red[i];
        gr = a > b ? a : b;
        a = object->green[i] - low[1];
        b = high[1] - object->green[i];
        gg = a > b ? a : b;
        a = object->blue[i] - low[2];
        b = high[2] - object->blue[i];
        gb = a > b ? a : b;
        far = gr * gr + gg * gg + gb * gb;
        if (far < bound) {
            bound = far;
        }
    }
    count = 0;
    for (i = 0; i < object->ncolors; ++i) {
        gr = object->red[i] < low[0] ? low[0] - object->red[i]
            : (object->red[i] > high[0] ? object->red[i] - high[0] : 0);
        gg = object->green[i] < low[1] ? low[1] - object->green[i]
            : (object->green[i] > high[1] ? object->green[i] - high[1] : 0);
        gb = object->blue[i] < low[2] ? low[2] - object->blue[i]
            : (object->blue[i] > high[2] ? object->blue[i] - high[2] : 0);
        near = gr * gr + gg * gg + gb * gb;
        if (near <= bound) {
            candidates[count++] = i;
        }
    }
    return count;
}

static void
sixel_lookup_policy_blockcull_map_pixels_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride, void *workspace)
{
    sixel_lookup_policy_blockcull_object_t const *object;
    unsigned char samples[SIXEL_LOOKUP_POLICY_BLOCKCULL_RUN][3];
    int candidates[SIXEL_PALETTE_MAX];
    int ncandidates;
    int low[3];
    int high[3];
    size_t done;
    size_t i;
    size_t n;
    int c;

    (void)workspace;
    if (policy == NULL || count == 0U) {
        return;
    }
    object = sixel_lookup_policy_blockcull_from_base_const(policy);
    if (object->prepared == 0) {
        sixel_lookup_policy_map_pixels_default(policy, pixels, count,
            pixel_stride, result, result_stride, workspace);
        return;
    }
    done = 0U;
    while (done < count) {
        n = count - done;
        if (n > SIXEL_LOOKUP_POLICY_BLOCKCULL_RUN) {
            n = SIXEL_LOOKUP_POLICY_BLOCKCULL_RUN;
        }
        for (c = 0; c < 3; ++c) {
            low[c] = 255;
            high[c] = 0;
        }
        for (i = 0; i < n; ++i) {
            for (c = 0; c < 3; ++c) {
                samples[i][c] = pixels[c];
                if (pixels[c] < low[c]) {
                    low[c] = pixels[c];
                }
                if (pixels[c] > high[c]) {
                    high[c] = pixels[c];
                }
            }
            /* Do not form a pointer before the buffer after a reverse row. */
            if (done + i + 1U < count) {
                pixels += pixel_stride;
            }
        }
        ncandidates = sixel_lookup_policy_blockcull_candidates_8bit(
            object, low, high, candidates);
        for (i = 0; i < n; ++i) {
            *result = (sixel_index_t)sixel_lookup_policy_blockcull_scan_8bit(
                object, samples[i][0], samples[i][1], samples[i][2],
                candidates, ncandidates);
            if (done + i + 1U < count) {
                result += result_stride;
            }
        }
        done += n;
    }
}

static int
sixel_lookup_policy_blockcull_map_pixel_float32(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_blockcull_object_t const *object;

    if (policy == NULL || pixel == NULL) {
        return 0;
    }
    object = sixel_lookup_policy_blockcull_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }
    return sixel_lookup_blockcull_float32_map(
        object->float_lookup, (float const *)(void const *)pixel);
}

static int
sixel_lookup_policy_blockcull_map_pixel_hint_float32(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel,
    int hint)
{
    (void)hint;
    return sixel_lookup_policy_blockcull_map_pixel_float32(policy, pixel);
}

static void
sixel_lookup_policy_blockcull_map_pixels_float32(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride, void *workspace)
{
    sixel_lookup_policy_blockcull_object_t const *object;

    if (policy == NULL || count == 0U) {
        return;
    }
    object = sixel_lookup_policy_blockcull_from_base_const(policy);
    if (object->prepared == 0) {
        sixel_lookup_policy_map_pixels_default(policy, pixels, count,
            pixel_stride, result, result_stride, workspace);
        return;
    }
    sixel_lookup_blockcull_float32_map_pixels(object->float_lookup,
        pixels, count, pixel_stride, result, result_stride);
}

static int
sixel_lookup_policy_blockcull_get_kind(
    sixel_lookup_policy_interface_t const *policy)
{
    (void)policy;
    return SIXEL_LUT_POLICY_BLOCKCULL;
}

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_blockcull_8bit_vtbl = {
    sixel_lookup_policy_blockcull_ref,
    sixel_lookup_policy_blockcull_unref,
    sixel_lookup_policy_blockcull_prepare_8bit,
    sixel_lookup_policy_blockcull_map_pixel_8bit,
    sixel_lookup_policy_blockcull_map_pixel_hint_8bit,
    sixel_lookup_policy_blockcull_map_pixels_8bit,
    NULL,
    sixel_lookup_policy_blockcull_get_kind,
};

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_blockcull_float32_vtbl = {
    sixel_lookup_policy_blockcull_ref,
    sixel_lookup_policy_blockcull_unref,
    sixel_lookup_policy_blockcull_prepare_float32,
    sixel_lookup_policy_blockcull_map_pixel_float32,
    sixel_lookup_policy_blockcull_map_pixel_hint_float32,
    sixel_lookup_policy_blockcull_map_pixels_float32,
    NULL,
    sixel_lookup_policy_blockcull_get_kind,
};

static SIXELSTATUS
sixel_lookup_policy_blockcull_new(
    sixel_allocator_t *allocator,
    sixel_lookup_policy_vtbl_t *vtbl,
    void **policy)
{
    sixel_lookup_policy_blockcull_object_t *object;

    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;
    object = (sixel_lookup_policy_blockcull_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_blockcull_new: allocation failed.");
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
sixel_lookup_policy_blockcull_8bit_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    return sixel_lookup_policy_blockcull_new(
        allocator, &g_sixel_lookup_policy_blockcull_8bit_vtbl, policy);
}

SIXELSTATUS
sixel_lookup_policy_blockcull_float32_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    return sixel_lookup_policy_blockcull_new(
        allocator, &g_sixel_lookup_policy_blockcull_float32_vtbl, policy);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
