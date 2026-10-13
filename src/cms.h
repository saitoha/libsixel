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

#ifndef LIBSIXEL_CMS_H
#define LIBSIXEL_CMS_H

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stddef.h>
#include "named-color.h"
#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_cms_profile sixel_cms_profile_t;
typedef struct sixel_cms_transform sixel_cms_transform_t;

typedef enum sixel_cms_engine {
    SIXEL_CMS_ENGINE_NONE = 0,
    SIXEL_CMS_ENGINE_AUTO = 1,
    SIXEL_CMS_ENGINE_BUILTIN = 2,
    SIXEL_CMS_ENGINE_LCMS2 = 3,
    SIXEL_CMS_ENGINE_COLORSYNC = 4
} sixel_cms_engine_t;

/* Stable values let the generic ordered-choice registry store CMS intent. */
typedef enum sixel_cms_rendering_intent {
    SIXEL_CMS_INTENT_PERCEPTUAL = 0,
    SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC = 1,
    SIXEL_CMS_INTENT_SATURATION = 2,
    SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC = 3
} sixel_cms_rendering_intent_t;

typedef enum sixel_cms_color_space {
    SIXEL_CMS_COLORSPACE_UNKNOWN = 0,
    SIXEL_CMS_COLORSPACE_RGB,
    SIXEL_CMS_COLORSPACE_GRAY,
    SIXEL_CMS_COLORSPACE_CMYK,
    SIXEL_CMS_COLORSPACE_LAB
} sixel_cms_color_space_t;

typedef enum sixel_cms_pixel_format {
    SIXEL_CMS_PIXELFORMAT_GRAY_8 = 1,
    SIXEL_CMS_PIXELFORMAT_RGB_8,
    SIXEL_CMS_PIXELFORMAT_RGBA_8,
    SIXEL_CMS_PIXELFORMAT_RGB_F32,
    SIXEL_CMS_PIXELFORMAT_CMYK_8,
    SIXEL_CMS_PIXELFORMAT_CMYK_16,
    /* Normalized device inks, not native LCMS percentages. */
    SIXEL_CMS_PIXELFORMAT_CMYK_F32,
    /* L/100, a/128, b/128, independently of the selected backend. */
    SIXEL_CMS_PIXELFORMAT_LAB_F32,
    SIXEL_CMS_PIXELFORMAT_GRAY_F32
} sixel_cms_pixel_format_t;

enum {
    SIXEL_CMS_TRANSFORM_DEFAULT = 0,
    SIXEL_CMS_TRANSFORM_COPY_ALPHA = 1
};

SIXEL_INTERNAL_API int
sixel_cms_engine_from_string(char const *text,
                             sixel_cms_engine_t *out_engine);

SIXEL_INTERNAL_API char const *
sixel_cms_engine_to_string(sixel_cms_engine_t engine);

SIXEL_INTERNAL_API void
sixel_cms_set_engine(sixel_cms_engine_t engine);

SIXEL_INTERNAL_API sixel_cms_engine_t
sixel_cms_get_engine(void);

SIXEL_INTERNAL_API int
sixel_cms_convert_profile_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  int pixelformat,
                                  sixel_cms_profile_t *src_profile);

SIXEL_INTERNAL_API int
sixel_cms_convert_to_srgb_with_profile_bytes(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length);

SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_linear_srgb_profile(void);

SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_cielab_d50_profile(void);

/* Convert float RGB, replicated gray or Lab to linear sRGB.
 * Lab uses L/100, a/128 and b/128 with a matching Lab source profile.
 * Publish only a complete finite result, retaining extended RGB values.
 * ColorSync RGB/Gray can use two disjoint preparation groups.
 * Failure leaves all input unchanged and never starts another group or
 * retries converted pixels. Other profiles retain one native apply. */
SIXEL_INTERNAL_API int
sixel_cms_convert_profile_to_linearrgb(unsigned char *pixels,
                                       int width,
                                       int height,
                                       int pixelformat,
                                       sixel_cms_profile_t *src_profile);

SIXEL_INTERNAL_API int
sixel_cms_convert_to_linearrgb_with_profile_bytes(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length);

/* CoreGraphics supplies either its adopted ICC or a verified named sRGB
 * definition (NULL bytes, zero length). This entry uses only ColorSync and
 * never reads or changes the requested CMS engine. Failure is atomic. */
SIXEL_INTERNAL_API int
sixel_cms_convert_colorsync_linear(float *pixels,
                                   int width,
                                   int height,
                                   int source_is_srgb,
                                   void const *profile_bytes,
                                   size_t profile_length);

SIXEL_INTERNAL_API int
sixel_cms_convert_profile_to_cielab(unsigned char *pixels,
                                    int width,
                                    int height,
                                    int pixelformat,
                                    sixel_cms_profile_t *src_profile);

SIXEL_INTERNAL_API int
sixel_cms_convert_to_cielab_with_profile_bytes(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length);

SIXEL_INTERNAL_API int
sixel_cms_convert_rgbf32_gamma_to_linear(float *pixels, size_t pixel_count);

SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_open_profile_from_mem(void const *data, size_t length);

SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_srgb_profile(void);

/* Container-independent H.273 mapping. Only full-range sRGB and Display P3
 * with the sRGB transfer are understood; NULL means an unsupported tuple. */
SIXEL_INTERNAL_API char const *
sixel_cms_cicp_name(unsigned char const codes[4]);

/* Build a named RGB source for float transforms in the requested engine.
 * Builtin retains the analytic definition; native engines consume an ICC
 * serialization. Failure never substitutes another CMS backend. */
SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_named_rgb_profile(char const *name);

SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_rgb_profile_from_gamma_chrm(double file_gamma,
                                             double white_x,
                                             double white_y,
                                             double red_x,
                                             double red_y,
                                             double green_x,
                                             double green_y,
                                             double blue_x,
                                             double blue_y);

SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_rgb_profile_from_gammas_chrm(double gamma_r,
                                              double gamma_g,
                                              double gamma_b,
                                              double white_x,
                                              double white_y,
                                              double red_x,
                                              double red_y,
                                              double green_x,
                                              double green_y,
                                              double blue_x,
                                              double blue_y);

SIXEL_INTERNAL_API sixel_cms_color_space_t
sixel_cms_get_color_space(sixel_cms_profile_t const *profile);

SIXEL_INTERNAL_API void
sixel_cms_close_profile(sixel_cms_profile_t *profile);

SIXEL_INTERNAL_API sixel_cms_transform_t *
sixel_cms_create_transform(sixel_cms_profile_t const *src_profile,
                           sixel_cms_pixel_format_t src_format,
                           sixel_cms_profile_t const *dst_profile,
                           sixel_cms_pixel_format_t dst_format,
                           int flags);

/* Known source formats and profile domains must agree for RGB_F32 output.
 * Creation may reject a mismatch; an existing transform rejects it before
 * any source/output write. UNKNOWN domains, invalid formats and other
 * destination formats retain their backend-specific rules.
 * LCMS rejects exact src == dst expansion from GRAY_8, RGB_8, RGBA_8,
 * CMYK_8, CMYK_16 or GRAY_F32 to RGB_F32 before any write, even for one
 * pixel. Separate buffers and the CMYK_F32/LAB_F32 alias adapters remain
 * supported. This rule does not define partial-overlap behavior.
 * ColorSync integer-promotion and LAB_F32 adapters to RGB_F32 require
 * matching native ICC source colorant counts, excluding RGBA alpha. An
 * unavailable or unequal native count fails before any source/output write;
 * matching UNKNOWN profile domains keep their native conversion paths. */
SIXEL_INTERNAL_API int
sixel_cms_do_transform(sixel_cms_transform_t const *transform,
                       void const *src,
                       void *dst,
                       size_t pixel_count);

/* Report the transform's actual backend, including profile-open fallback. */
SIXEL_INTERNAL_API sixel_cms_engine_t
sixel_cms_transform_engine(sixel_cms_transform_t const *transform);

SIXEL_INTERNAL_API void
sixel_cms_delete_transform(sixel_cms_transform_t *transform);

/*
 * Job-local color definitions are independent of a frame's working-space
 * tag. A transform owns clones of both definitions. No profile byte buffer
 * or definition supplied by its caller must outlive transform creation.
 *
 * This internal foundation currently uses the builtin ICC PCS evaluator.
 * It does not change loader defaults or select a terminal color policy.
 */
typedef struct sixel_color_def sixel_color_def_t;
typedef struct sixel_color_xform sixel_color_xform_t;

typedef enum sixel_color_trc {
    SIXEL_COLOR_TRC_SRGB = 0,
    SIXEL_COLOR_TRC_POWER = 1
} sixel_color_trc_t;

/* The same registry drives encoder intent and decoder/profile export. */
SIXEL_INTERNAL_API char const *
sixel_cdef_named_name(int identity);

SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_name(char const *name);

/* xy order: white, red, green, blue. Gamma is the decoding exponent. */
SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_rgb(double const xy[8], sixel_color_trc_t trc,
                double const gamma[3]);

SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_icc(void const *bytes, size_t length);

/* Strict shared input grammar. On success, the caller owns *definition;
 * auto and icc:FILE leave it NULL. ICC paths borrow the input string.
 * No file I/O occurs here; the adapter loads and validates ICC bytes. */
SIXEL_INTERNAL_API int
sixel_cdef_parse(char const *text, sixel_color_def_t **definition,
                  char const **icc_path, int *intent);

/* Serialize a shared RGB definition without applying it to pixels.
 * allocator must be nonnull; it owns the returned byte buffer. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_cdef_export_icc(sixel_color_def_t const *definition,
                      unsigned char **bytes, size_t *length,
                      sixel_allocator_t *allocator);

SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_clone(sixel_color_def_t const *definition);

/* Analytic and matrix/TRC RGB models have separable channel transfers.
 * ICC models with LUTs are excluded: a LUT need not be additive.
 * Linearization owns its result; transfer publishes only finite results,
 * preserves extended analytic coordinates; ICC curves retain their
 * specified device-domain boundary. */
SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_linearized(sixel_color_def_t const *definition);

SIXEL_INTERNAL_API int
sixel_cdef_transfer(sixel_color_def_t const *definition,
                     double const input[3], double output[3], int encode);

SIXEL_INTERNAL_API void
sixel_cdef_free(sixel_color_def_t *definition);

/* XYZ is D50 PCS (Y=1); an ICC slot supplies its rendering semantics.
 * No additional black-point compensation or absolute adaptation is added.
 * Analytic definitions use relative colorimetry, never clipped to sRGB. */
SIXEL_INTERNAL_API int
sixel_cdef_to_xyz(sixel_color_def_t const *definition,
                   double const rgb[3], double xyz[3], int intent);

SIXEL_INTERNAL_API int
sixel_cdef_from_xyz(sixel_color_def_t const *definition,
                     double const xyz[3], double rgb[3], int intent);

/* This checks actual apply in both directions, not mathematical inversion. */
SIXEL_INTERNAL_API int
sixel_cdef_output_ready(sixel_color_def_t const *definition,
                         int encoding_intent, int evaluation_intent);

SIXEL_INTERNAL_API sixel_color_xform_t *
sixel_cxform_new(sixel_color_def_t const *source,
                  sixel_color_def_t const *destination, int intent);

SIXEL_INTERNAL_API int
sixel_cxform_apply(sixel_color_xform_t const *transform,
                    double const source[3], double destination[3]);

SIXEL_INTERNAL_API void
sixel_cxform_free(sixel_color_xform_t *transform);

#ifdef __cplusplus
}
#endif

#endif


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
