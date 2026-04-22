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

#ifndef LIBSIXEL_LOOKUP_POLICY_H
#define LIBSIXEL_LOOKUP_POLICY_H

#include <sixel.h>

#include "lookup-common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sixel_lookup_policy_mode {
    SIXEL_DITHER_LOOKUP_MODE_NORMAL = 0,
    SIXEL_DITHER_LOOKUP_MODE_FAST_LUT,
    SIXEL_DITHER_LOOKUP_MODE_MONO_DARKBG,
    SIXEL_DITHER_LOOKUP_MODE_MONO_LIGHTBG
} sixel_lookup_policy_mode_t;

typedef struct sixel_lookup_policy sixel_lookup_policy_t;

typedef int (*sixel_lookup_policy_map_fn)(
    sixel_lookup_policy_t const *policy,
    unsigned char const *pixel);

typedef struct sixel_lookup_policy_prepare_request {
    unsigned char const *palette;
    float const *palette_float;
    int depth;
    int float_depth;
    int reqcolor;
    int complexion;
    int optimize_lookup;
    int lut_policy;
    int method_for_largest;
    int pixelformat;
    int parallel_active;
    int reuse_lut_is_shared;
    int reuse_lut_preconfigured;
    sixel_lut_t *reuse_lut;
    sixel_lut_t **reuse_lut_slot;
    sixel_allocator_t *allocator;
} sixel_lookup_policy_prepare_request_t;

typedef int (*sixel_lookup_policy_lookup_fn)(
    unsigned char const *pixel,
    int depth,
    unsigned char const *palette,
    int reqcolor,
    unsigned short *cachetable,
    int complexion);

typedef struct sixel_lookup_policy_vtbl {
    char const *name;
    SIXELSTATUS (*prepare)(sixel_lookup_policy_t *policy,
                           sixel_lookup_policy_prepare_request_t const *
                               request);
    int (*map_pixel)(sixel_lookup_policy_t const *policy,
                     unsigned char const *pixel);
} sixel_lookup_policy_vtbl_t;

struct sixel_lookup_policy {
    sixel_lookup_policy_vtbl_t const *vtbl;
    sixel_lookup_policy_mode_t mode;
    sixel_lookup_policy_lookup_fn lookup_fn;
    unsigned char const *palette;
    float const *palette_float;
    int depth;
    int float_depth;
    int reqcolor;
    int complexion;
    int pixelformat;
    int lut_policy;
    int method_for_largest;
    unsigned short *indextable;
    sixel_lut_t *lut;
    int owns_lut;
    int lookup_source_is_float;
    int prefer_palette_float_lookup;
};

SIXEL_INTERNAL_API void
sixel_lookup_policy_init(sixel_lookup_policy_t *policy);

SIXEL_INTERNAL_API void
sixel_lookup_policy_clear(sixel_lookup_policy_t *policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_prepare(sixel_lookup_policy_t *policy,
                            sixel_lookup_policy_prepare_request_t const *
                                request);

SIXEL_INTERNAL_API sixel_lookup_policy_mode_t
sixel_lookup_policy_get_mode(sixel_lookup_policy_t const *policy);

SIXEL_INTERNAL_API sixel_lookup_policy_map_fn
sixel_lookup_policy_get_map_fn(sixel_lookup_policy_t const *policy);

SIXEL_INTERNAL_API int
sixel_lookup_policy_map_pixel(sixel_lookup_policy_t const *policy,
                              unsigned char const *pixel);

SIXEL_INTERNAL_API int
sixel_lookup_policy_lookup_source_is_float(sixel_lookup_policy_t const *policy);

SIXEL_INTERNAL_API int
sixel_lookup_policy_prefer_palette_float_lookup(
    sixel_lookup_policy_t const *policy);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOOKUP_POLICY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
