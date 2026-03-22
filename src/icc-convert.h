/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_ICC_CONVERT_H
#define LIBSIXEL_ICC_CONVERT_H

#include <stddef.h>

#include <sixel.h>

int
sixel_icc_convert_to_srgb_with_pixelformat(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length);

#endif  /* LIBSIXEL_ICC_CONVERT_H */
