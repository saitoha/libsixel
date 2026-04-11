/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FROMBMP_INTERNAL_H
#define LIBSIXEL_FROMBMP_INTERNAL_H

#include <stddef.h>
#include <sixel.h>

#include "chunk.h"

#define SIXEL_BMP_MAX_PALETTE 256u

typedef struct sixel_bmp_decode_info {
    sixel_chunk_t const *chunk;
    int width;
    int height;
    int top_down;
    int bpp;
    int is_cmyk;
    int dib_family;
    unsigned int compression;
    unsigned int red_mask;
    unsigned int green_mask;
    unsigned int blue_mask;
    unsigned int alpha_mask;
    int has_alpha_mask;
    int has_explicit_alpha;
    int palette_count;
    unsigned char palette[SIXEL_BMP_MAX_PALETTE][4];
    size_t pixel_offset;
    size_t payload_size;
    size_t row_stride;
    unsigned char const *payload;
    unsigned char const *icc_profile;
    size_t icc_profile_length;
    int has_calibrated_rgb;
    double calibrated_gamma;
    double calibrated_gamma_r;
    double calibrated_gamma_g;
    double calibrated_gamma_b;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
} sixel_bmp_decode_info_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_bmp_parse_header(sixel_chunk_t const *chunk,
                       sixel_bmp_decode_info_t *info,
                       int info40_mode);

#endif /* LIBSIXEL_FROMBMP_INTERNAL_H */

