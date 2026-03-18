/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_ICC_PARSE_H
#define LIBSIXEL_ICC_PARSE_H

#include <stddef.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif

typedef enum sixel_icc_curve_kind {
    SIXEL_ICC_CURVE_INVALID = 0,
    SIXEL_ICC_CURVE_IDENTITY,
    SIXEL_ICC_CURVE_GAMMA,
    SIXEL_ICC_CURVE_TABLE
} sixel_icc_curve_kind_t;

typedef struct sixel_icc_curve {
    sixel_icc_curve_kind_t kind;
    double gamma;
    uint16_t *table;
    size_t table_length;
} sixel_icc_curve_t;

typedef enum sixel_icc_profile_kind {
    SIXEL_ICC_PROFILE_KIND_INVALID = 0,
    SIXEL_ICC_PROFILE_KIND_RGB,
    SIXEL_ICC_PROFILE_KIND_GRAY
} sixel_icc_profile_kind_t;

typedef struct sixel_icc_profile {
    sixel_icc_profile_kind_t kind;
    double matrix_to_xyz_d50[3][3];
    double gray_white_xyz_d50[3];
    sixel_icc_curve_t curves[3];
} sixel_icc_profile_t;

int
sixel_icc_parse_profile(void const *data,
                        size_t length,
                        sixel_icc_profile_t *out_profile);

int
sixel_icc_parse_png_iccp(unsigned char const *png_data,
                         size_t png_size,
                         sixel_icc_profile_t *out_profile);

void
sixel_icc_profile_destroy(sixel_icc_profile_t *profile);

#endif
