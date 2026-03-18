/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_ICC_APPLY_H
#define LIBSIXEL_ICC_APPLY_H

#include <stddef.h>

#include "icc-parse.h"

int
sixel_icc_apply_rgb_u8(unsigned char *pixels,
                       size_t pixel_count,
                       sixel_icc_profile_t const *profile);

int
sixel_icc_apply_rgb_float32(float *pixels,
                            size_t pixel_count,
                            sixel_icc_profile_t const *profile);

int
sixel_icc_apply_gray_u8(unsigned char *pixels,
                        size_t pixel_count,
                        sixel_icc_profile_t const *profile);

int
sixel_icc_apply_rgb_triplet_unit(double rgb[3],
                                 sixel_icc_profile_t const *profile);

#endif
