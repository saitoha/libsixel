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

#ifndef LIBSIXEL_TARGET_COLOR_H
#define LIBSIXEL_TARGET_COLOR_H

#include "cms.h"
#include "gamut.h"

/*
 * Target color policy.
 *
 * Lookup and clustering consume the source expressed in -W or -X. The
 * target is the color the -U receiver reproduces for that source: encode
 * with the output transform, bound the wire code range, interpret the code
 * through the receiver definition, then return to -W or -X. Two deviations
 * separate it from the source: (a) colors outside the -U gamut, and
 * (b) encode/interpret pairs that are not exact inverses (ICC A2B/B2A
 * asymmetry, approximated TRC inverses). Wire quantization is excluded.
 */
enum {
    SIXEL_TARGET_COLOR_POLICY_SOURCE = 0,  /* no correction (default) */
    SIXEL_TARGET_COLOR_POLICY_GAMUT,       /* (a) only */
    SIXEL_TARGET_COLOR_POLICY_FULL,        /* (a) and (b) */
    SIXEL_TARGET_COLOR_POLICY_AUTO         /* measure (b), pick gamut/full */
};

enum {
    SIXEL_TARGET_COLOR_METHOD_EXACT = 0,   /* per-pixel transforms */
    SIXEL_TARGET_COLOR_METHOD_LUT,         /* verified 3D LUT, exact edges */
    SIXEL_TARGET_COLOR_METHOD_AUTO         /* LUT when pixels dominate */
};

#define SIXEL_TARGET_COLOR_LUT_SIZE_DEFAULT 33
#define SIXEL_TARGET_COLOR_LUT_SIZE_MIN 5
#define SIXEL_TARGET_COLOR_LUT_SIZE_MAX 65
#define SIXEL_TARGET_COLOR_TOLERANCE_DEFAULT 0.001
#define SIXEL_TARGET_COLOR_TOLERANCE_MAX 0.1

/* Offsets below this magnitude are round-off of the conversion pair, the
 * negligible deviation the policy excludes. Snapping them to zero lets an
 * identity pair drop its offset plane instead of diffusing noise. */
#define SIXEL_TARGET_COLOR_OFFSET_EPSILON 1e-6

typedef struct sixel_target_color_options {
    int policy;
    int method;
    int lut_size;
    double tolerance;      /* Delta E OK */
} sixel_target_color_options_t;

typedef struct sixel_target_color sixel_target_color_t;

/* Owned, immutable after construction except for the auto-stage cache,
 * which is filled by the thread that owns the encode. The output
 * definition is cloned; the gamut mapper is cloned when supplied. A NULL
 * mapper bounds out-of-gamut colors by clipping wire codes. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_target_color_new(sixel_target_color_t **out,
                       sixel_target_color_options_t const *options,
                       int output_definition,
                       int output_colorspace,
                       sixel_color_def_t const *output_icc,
                       sixel_gamut_t const *gamut,
                       sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_target_color_clone(sixel_target_color_t const *source,
                         sixel_target_color_t **out);

SIXEL_INTERNAL_API void
sixel_target_color_free(sixel_target_color_t *target);

/* Copy the construction options (policy, method, LUT size, tolerance). */
SIXEL_INTERNAL_API void
sixel_target_color_get_options(sixel_target_color_t const *target,
                               sixel_target_color_options_t *options);

/* True when the -U encode/interpret pair is the identity in COLORSPACE:
 * a builtin destination with the same definition. Values inside [0,1]
 * then have themselves as targets under every policy. */
SIXEL_INTERNAL_API int
sixel_target_color_is_identity(sixel_target_color_t const *target,
                               int colorspace);

/* Resolve auto to gamut or full for COLORSPACE. Other policies return
 * themselves. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_target_color_resolve(sixel_target_color_t *target, int colorspace,
                           int *policy);

/* Evaluate one triplet exactly with a resolved policy. */
SIXEL_INTERNAL_API int
sixel_target_color_apply(sixel_target_color_t const *target, int policy,
                         double const input[3], double output[3],
                         int colorspace);

/* Write the target minus the source for COUNT packed float triplets.
 * Hidden pixels receive a zero offset. *nonzero reports whether any
 * offset differs from zero, so callers can drop an all-zero plane. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_target_color_offset_rgbf32(sixel_target_color_t *target,
                                 float const *pixels, size_t count,
                                 int colorspace,
                                 unsigned char const *hidden,
                                 size_t hidden_count,
                                 float *offset, int *nonzero);

/* Replace COUNT packed float triplets by their targets in place. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_target_color_map_rgbf32(sixel_target_color_t *target,
                              float *pixels, size_t count, int colorspace,
                              unsigned char const *hidden,
                              size_t hidden_count);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
