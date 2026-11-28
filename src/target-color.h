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
 * Gamut targets.
 *
 * Lookup consumes the source expressed in -W. A source outside the -U
 * gamut cannot be shown by the receiver; left as the goal of diffusion it
 * keeps feeding error that no palette color can absorb, and as a
 * clustering sample it spends a palette slot on an unreachable color. The
 * aim of such a color is what the receiver shows for its mapped,
 * wire-clamped code; a representable color is its own aim. Two -U
 * suboptions use it independently of the ordinary gamut mapping:
 *
 *   diffusion=aim  lookup keeps the source while diffusion aims at the
 *                  aim, instead of mapping the lookup input;
 *   samples=shown  clustering samples become their aims, instead of
 *                  their mapper replacements.
 *
 * Deviations of the encode/interpret pair inside the gamut need no aim:
 * explicit ICC output already evaluates every emitted register through the
 * device-to-PCS transform and gives lookup and diffusion that shown color.
 * Wire quantization is excluded throughout.
 */
enum {
    SIXEL_TARGET_COLOR_AIM_SOURCE = 0,     /* diffusion=input (default) */
    SIXEL_TARGET_COLOR_AIM_GAMUT           /* diffusion=aim */
};

enum {
    SIXEL_TARGET_COLOR_CLUSTER_SOURCE = 0, /* samples=mapped (default) */
    SIXEL_TARGET_COLOR_CLUSTER_GAMUT       /* samples=shown */
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

/* Offsets below this magnitude are round-off of the conversion pair, the
 * negligible deviation an aim excludes. Snapping them to zero lets an
 * identity pair drop its offset plane instead of diffusing noise. */
#define SIXEL_TARGET_COLOR_OFFSET_EPSILON 1e-6

typedef struct sixel_target_color_options {
    int aim;
    int cluster;
    int method;
    int lut_size;
    double tolerance;      /* Delta E OK for LUT cell verification */
} sixel_target_color_options_t;

typedef struct sixel_target_color_axes {
    int aim;
    int cluster;
} sixel_target_color_axes_t;

typedef struct sixel_target_color sixel_target_color_t;

/* Owned and immutable after construction. The output definition is
 * cloned; the gamut mapper is cloned when supplied. A NULL mapper bounds
 * out-of-gamut colors by clipping wire codes. */
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

/* Copy the construction options. */
SIXEL_INTERNAL_API void
sixel_target_color_get_options(sixel_target_color_t const *target,
                               sixel_target_color_options_t *options);

/* Copy the resolved axes. A NULL target reports the source behavior. */
SIXEL_INTERNAL_API void
sixel_target_color_get_axes(sixel_target_color_t const *target,
                            sixel_target_color_axes_t *axes);

/* True when the -U encode/interpret pair is the identity in COLORSPACE:
 * a builtin destination with the same definition. Values inside [0,1]
 * then are their own aim. */
SIXEL_INTERNAL_API int
sixel_target_color_is_identity(sixel_target_color_t const *target,
                               int colorspace);

/* Write the aim minus the source for COUNT packed float triplets. A color
 * inside the -U gamut is its own aim. Hidden pixels receive a zero
 * offset. *nonzero reports whether any offset differs from zero, so
 * callers can drop an all-zero plane. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_target_color_offset_rgbf32(sixel_target_color_t *target,
                                 float const *pixels, size_t count,
                                 int colorspace,
                                 unsigned char const *hidden,
                                 size_t hidden_count,
                                 float *offset, int *nonzero);

/* Replace COUNT packed float triplets by their aim colors in place. */
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
