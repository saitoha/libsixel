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

#include "lookup-policy-gridlut.h"
#include "lookup-policy-blockcull.h"
#include "sixel_atomic.h"


/*
 * IDL (internal contract)
 *
 * class LookupGridlut : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   map_pixels(pixels);
 * }
 *
 * Byte queries use a 16x16x16 RGB grid. Each cell keeps every color
 * whose nearest box distance is no greater than the smallest farthest
 * corner distance. The query is never rounded: only candidates are culled.
 * Up to 64 candidates are stored in palette order; overflowing cells scan
 * the whole palette. Float queries retain the exact blockcull backend.
 */

enum {
    SIXEL_LOOKUP_POLICY_GRIDLUT_COMPONENTS = 3
};

typedef struct sixel_lookup_policy_gridlut_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int prepared;
    int ncolors;
    /* Byte class: palette channels as separate integer arrays. */
    int red[SIXEL_PALETTE_MAX];
    int green[SIXEL_PALETTE_MAX];
    int blue[SIXEL_PALETTE_MAX];
    /* A zero count means overflow: scan the full palette, never truncate. */
    unsigned char counts[4096];
    unsigned char candidates[4096][64];
} sixel_lookup_policy_gridlut_object_t;

static sixel_lookup_policy_gridlut_object_t *
sixel_lookup_policy_gridlut_from_base(
    sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_gridlut_object_t *)(void *)policy;
}

static sixel_lookup_policy_gridlut_object_t const *
sixel_lookup_policy_gridlut_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_gridlut_object_t const *)(void const *)
        policy;
}

static void
sixel_lookup_policy_gridlut_reset(
    sixel_lookup_policy_gridlut_object_t *object)
{
    object->prepared = 0;
    object->ncolors = 0;
}

static void
sixel_lookup_policy_gridlut_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_gridlut_object_t *object;

    if (policy == NULL) {
        return;
    }
    object = sixel_lookup_policy_gridlut_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_gridlut_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_gridlut_object_t *object;
    sixel_allocator_t *allocator;

    if (policy == NULL) {
        return;
    }
    object = sixel_lookup_policy_gridlut_from_base(policy);
    if (sixel_atomic_fetch_sub_u32(&object->ref, 1U) == 1U) {
        sixel_lookup_policy_gridlut_reset(object);
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
sixel_lookup_policy_gridlut_check_request(
    sixel_lookup_policy_prepare_request_t const *request)
{
    if (request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->depth != SIXEL_LOOKUP_POLICY_GRIDLUT_COMPONENTS) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->reqcolor > SIXEL_PALETTE_MAX) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_gridlut: palette exceeds 256 colors.");
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

/* Keep color i when its box distance does not exceed the smallest
 * farthest-corner distance over all colors. Integer arithmetic is exact. */
static int
sixel_lookup_policy_gridlut_candidates_8bit(
    sixel_lookup_policy_gridlut_object_t const *object,
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

static SIXELSTATUS
sixel_lookup_policy_gridlut_prepare_8bit(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_gridlut_object_t *object;
    SIXELSTATUS status;
    int i;
    int cell;
    int count;
    int low[3];
    int high[3];
    int candidates[SIXEL_PALETTE_MAX];

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    object = sixel_lookup_policy_gridlut_from_base(policy);
    status = sixel_lookup_policy_gridlut_check_request(request);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_gridlut_reset(object);
        return status;
    }
    /* Palette identity, not a reuse hint, controls the immutable table. */
    if (object->prepared && object->ncolors == request->reqcolor) {
        for (i = 0; i < request->reqcolor; ++i) {
            if (object->red[i] != request->palette[i * 3 + 0]
                    || object->green[i] != request->palette[i * 3 + 1]
                    || object->blue[i] != request->palette[i * 3 + 2]) {
                break;
            }
        }
        if (i == request->reqcolor) {
            return SIXEL_OK;
        }
    }
    sixel_lookup_policy_gridlut_reset(object);
    for (i = 0; i < request->reqcolor; ++i) {
        object->red[i] = request->palette[i * 3 + 0];
        object->green[i] = request->palette[i * 3 + 1];
        object->blue[i] = request->palette[i * 3 + 2];
    }
    object->ncolors = request->reqcolor;
    for (cell = 0; cell < 4096; ++cell) {
        low[0] = (cell >> 8) * 16;
        low[1] = ((cell >> 4) & 15) * 16;
        low[2] = (cell & 15) * 16;
        for (i = 0; i < 3; ++i) {
            high[i] = low[i] + 15;
        }
        count = sixel_lookup_policy_gridlut_candidates_8bit(
            object, low, high, candidates);
        object->counts[cell] = (unsigned char)(count <= 64 ? count : 0);
        for (i = 0; i < object->counts[cell]; ++i) {
            object->candidates[cell][i] = (unsigned char)candidates[i];
        }
    }
    object->prepared = 1;
    return SIXEL_OK;
}

/* Strict first minimum over the listed colors, or over the whole palette
 * when candidates is NULL: equal distances keep the lowest index. */
static int
sixel_lookup_policy_gridlut_scan_8bit(
    sixel_lookup_policy_gridlut_object_t const *object,
    int r, int g, int b, unsigned char const *candidates, int ncandidates)
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
sixel_lookup_policy_gridlut_map_pixel_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_gridlut_object_t const *object;
    int cell;
    int count;

    if (policy == NULL || pixel == NULL) {
        return 0;
    }
    object = sixel_lookup_policy_gridlut_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }
    cell = ((int)(pixel[0] >> 4) << 8) |
        ((int)(pixel[1] >> 4) << 4) | (pixel[2] >> 4);
    count = object->counts[cell];
    if (count == 1) {
        return object->candidates[cell][0];
    }
    return sixel_lookup_policy_gridlut_scan_8bit(
        object, pixel[0], pixel[1], pixel[2],
        count != 0 ? object->candidates[cell] : NULL,
        count != 0 ? count : object->ncolors);
}

/* Cell candidates do not depend on the previous pixel or its hint. */
static int
sixel_lookup_policy_gridlut_map_pixel_hint_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel,
    int hint)
{
    (void)hint;
    return sixel_lookup_policy_gridlut_map_pixel_8bit(policy, pixel);
}

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_gridlut_8bit_vtbl = {
    sixel_lookup_policy_gridlut_ref,
    sixel_lookup_policy_gridlut_unref,
    sixel_lookup_policy_gridlut_prepare_8bit,
    sixel_lookup_policy_gridlut_map_pixel_8bit,
    sixel_lookup_policy_gridlut_map_pixel_hint_8bit,
    sixel_lookup_policy_map_pixels_default,
    NULL,
};

static SIXELSTATUS
sixel_lookup_policy_gridlut_new(
    sixel_allocator_t *allocator,
    sixel_lookup_policy_vtbl_t *vtbl,
    void **policy)
{
    sixel_lookup_policy_gridlut_object_t *object;

    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;
    object = (sixel_lookup_policy_gridlut_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_gridlut_new: allocation failed.");
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
sixel_lookup_policy_gridlut_8bit_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    return sixel_lookup_policy_gridlut_new(
        allocator, &g_sixel_lookup_policy_gridlut_8bit_vtbl, policy);
}

SIXELSTATUS
sixel_lookup_policy_gridlut_float32_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    /* A byte-domain grid cannot bound arbitrary float working coordinates.
     * Reuse the established exact backend without allocating a byte table. */
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
