/*
 * SPDX-License-Identifier: MIT
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

#endif
