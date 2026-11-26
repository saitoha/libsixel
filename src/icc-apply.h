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

#ifndef LIBSIXEL_ICC_APPLY_H
#define LIBSIXEL_ICC_APPLY_H

#include <stddef.h>
#include <sixel.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include "icc-parse.h"

SIXEL_INTERNAL_API int
sixel_icc_apply_rgb_u8(unsigned char *pixels,
                       size_t pixel_count,
                       sixel_icc_profile_t const *profile);

SIXEL_INTERNAL_API int
sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(
    double xyz_d50[3],
    double const *device_unit,
    size_t input_channel_count,
    sixel_icc_profile_t const *profile,
    unsigned int a2b_slot,
    int allow_matrix_trc_fallback);

SIXEL_INTERNAL_API int
sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(
    double *device_unit,
    size_t output_channel_count,
    double const xyz_d50[3],
    sixel_icc_profile_t const *profile,
    unsigned int b2a_slot);

SIXEL_INTERNAL_API int
sixel_icc_apply_rgb_u8_with_a2b_slot(
    unsigned char *pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    unsigned int a2b_slot,
    int allow_matrix_trc_fallback);

SIXEL_INTERNAL_API int
sixel_icc_apply_rgb_float32(float *pixels,
                            size_t pixel_count,
                            sixel_icc_profile_t const *profile);

SIXEL_INTERNAL_API int
sixel_icc_apply_rgb_float32_with_a2b_slot(
    float *pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    unsigned int a2b_slot,
    int allow_matrix_trc_fallback);

SIXEL_INTERNAL_API int
sixel_icc_apply_gray_u8(unsigned char *pixels,
                        size_t pixel_count,
                        sixel_icc_profile_t const *profile);

SIXEL_INTERNAL_API int
sixel_icc_apply_gray_u8_with_a2b_slot(
    unsigned char *pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    unsigned int a2b_slot,
    int allow_matrix_trc_fallback);

SIXEL_INTERNAL_API int
sixel_icc_apply_rgb_triplet_unit(double rgb[3],
                                 sixel_icc_profile_t const *profile);

SIXEL_INTERNAL_API int
sixel_icc_apply_rgb_triplet_unit_with_a2b_slot(
    double rgb[3],
    sixel_icc_profile_t const *profile,
    unsigned int a2b_slot,
    int allow_matrix_trc_fallback);

SIXEL_INTERNAL_API int
sixel_icc_apply_cmyk_u8_to_rgb_float32(float *dst_pixels,
                                       unsigned char const *src_pixels,
                                       size_t pixel_count,
                                       sixel_icc_profile_t const *profile);

SIXEL_INTERNAL_API int
sixel_icc_apply_cmyk_u8_to_rgb_float32_with_a2b_slot(
    float *dst_pixels,
    unsigned char const *src_pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    unsigned int a2b_slot);

SIXEL_INTERNAL_API int
sixel_icc_apply_cmyk_u16_to_rgb_float32(float *dst_pixels,
                                        uint16_t const *src_pixels,
                                        size_t pixel_count,
                                        sixel_icc_profile_t const *profile);

SIXEL_INTERNAL_API int
sixel_icc_apply_cmyk_u16_to_rgb_float32_with_a2b_slot(
    float *dst_pixels,
    uint16_t const *src_pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    unsigned int a2b_slot);

SIXEL_INTERNAL_API int
sixel_icc_apply_cmyk_float32_to_rgb_float32(float *dst_pixels,
                                            float const *src_pixels,
                                            size_t pixel_count,
                                            sixel_icc_profile_t const *profile);

SIXEL_INTERNAL_API int
sixel_icc_apply_cmyk_float32_to_rgb_float32_with_a2b_slot(
    float *dst_pixels,
    float const *src_pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    unsigned int a2b_slot);

SIXEL_INTERNAL_API int
sixel_icc_curve_device(sixel_icc_curve_t const *curve, double value,
                          int inverse, double *out);

SIXEL_INTERNAL_API int
sixel_icc_curve_intermediate(sixel_icc_curve_t const *curve, double value,
                             int inverse, double *out);

/* Evaluate one parsed cvst element. Discard output on failure. */
SIXEL_INTERNAL_API int
sixel_icc_apply_mpe_curves(double *out, double const *in, size_t channels,
                          sixel_icc_mpet_t const *mpet, size_t element);

/* Evaluate one parsed curve set, matrix or CLUT with distinct I/O buffers.
 * At the PCS boundary the values are XYZ or L*, a*, b* directly.
 * Discard the output on failure. */
SIXEL_INTERNAL_API int
sixel_icc_apply_mpe_element(double *out, size_t output_channels,
                            double const *in, size_t input_channels,
                            sixel_icc_mpet_t const *mpet, size_t element);

/* Signed double area of a triangle of chromaticity coordinates. */
SIXEL_INTERNAL_API double
sixel_icc_xy_area(double const a[2], double const b[2], double const p[2]);

/* Bridge to physical PCS XYZ before applying an intent adjustment. */
typedef enum {
    SIXEL_ICC_PATH_NONE,
    SIXEL_ICC_PATH_LEGACY,
    SIXEL_ICC_PATH_MPET
} sixel_icc_path_t;

/* These two entries accept and return Lab in facade float units
 * (L* / 100, signed a* / 128, signed b* / 128).
 * A forward slot of SIXEL_ICC_MPET_SLOT_COUNT requests analytic Lab. */
SIXEL_INTERNAL_API sixel_icc_path_t
sixel_icc_device_to_pcs(double xyz[3], double const *device, size_t channels,
                         sixel_icc_profile_t const *profile,
                         unsigned int slot, int legacy_only);

SIXEL_INTERNAL_API sixel_icc_path_t
sixel_icc_pcs_to_device(double *device, size_t channels, double const xyz[3],
                         sixel_icc_profile_t const *profile,
                         unsigned int slot, int legacy_only);

SIXEL_INTERNAL_API int
sixel_icc_apply_lab_xyz_to_device(double device[3], double const xyz[3],
                                   sixel_icc_profile_t const *profile,
                                   int lab_float);

/* The boolean entry points use the legacy unsigned Lab encoding. */
SIXEL_INTERNAL_API int
sixel_icc_apply_device_to_physical_xyz(
    double xyz[3], double const *device, size_t channels,
    sixel_icc_profile_t const *profile, unsigned int slot);

SIXEL_INTERNAL_API int
sixel_icc_apply_physical_xyz_to_device(
    double *device, size_t channels, double const xyz[3],
    sixel_icc_profile_t const *profile, unsigned int slot);

#endif


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
