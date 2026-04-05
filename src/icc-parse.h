/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_ICC_PARSE_H
#define LIBSIXEL_ICC_PARSE_H

#include <stddef.h>
#include <sixel.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#define SIXEL_ICC_A2B_SLOT_COUNT 3u
#define SIXEL_ICC_B2A_SLOT_COUNT SIXEL_ICC_A2B_SLOT_COUNT

typedef enum sixel_icc_curve_kind {
    SIXEL_ICC_CURVE_INVALID = 0,
    SIXEL_ICC_CURVE_IDENTITY,
    SIXEL_ICC_CURVE_GAMMA,
    SIXEL_ICC_CURVE_TABLE,
    SIXEL_ICC_CURVE_SEGM_TABLE
} sixel_icc_curve_kind_t;

typedef struct sixel_icc_curve {
    sixel_icc_curve_kind_t kind;
    double gamma;
    uint16_t *table;
    size_t table_length;
} sixel_icc_curve_t;

typedef enum sixel_icc_lut_kind {
    SIXEL_ICC_LUT_INVALID = 0,
    SIXEL_ICC_LUT_MFT1,
    SIXEL_ICC_LUT_MFT2,
    SIXEL_ICC_LUT_MFT1_RGB_GRAY_A2B0,
    SIXEL_ICC_LUT_MFT2_RGB_GRAY_A2B0
} sixel_icc_lut_kind_t;

typedef struct sixel_icc_lut {
    sixel_icc_lut_kind_t kind;
    uint8_t input_channels;
    uint8_t output_channels;
    uint8_t clut_grid_points;
    uint16_t input_entries;
    uint16_t output_entries;
    uint16_t *input_tables;
    uint16_t *clut_values;
    uint16_t *output_tables;
} sixel_icc_lut_t;

typedef enum sixel_icc_mab_type {
    SIXEL_ICC_MAB_TYPE_INVALID = 0,
    SIXEL_ICC_MAB_TYPE_MAB,
    SIXEL_ICC_MAB_TYPE_MBA
} sixel_icc_mab_type_t;

typedef struct sixel_icc_mab_clut {
    uint8_t input_channels;
    uint8_t output_channels;
    uint8_t grid_points[16];
    uint16_t *values;
    size_t value_count;
} sixel_icc_mab_clut_t;

typedef struct sixel_icc_mab_pipeline {
    sixel_icc_mab_type_t type;
    uint8_t input_channels;
    uint8_t output_channels;
    int has_a_curves;
    int has_m_curves;
    int has_b_curves;
    int has_clut;
    int has_matrix;
    sixel_icc_curve_t a_curves[16];
    sixel_icc_curve_t m_curves[16];
    sixel_icc_curve_t b_curves[16];
    sixel_icc_mab_clut_t clut;
    double matrix[3][3];
    double matrix_offset[3];
} sixel_icc_mab_pipeline_t;

typedef enum sixel_icc_profile_pcs {
    SIXEL_ICC_PROFILE_PCS_INVALID = 0,
    SIXEL_ICC_PROFILE_PCS_XYZ,
    SIXEL_ICC_PROFILE_PCS_LAB
} sixel_icc_profile_pcs_t;

typedef enum sixel_icc_profile_kind {
    SIXEL_ICC_PROFILE_KIND_INVALID = 0,
    SIXEL_ICC_PROFILE_KIND_RGB,
    SIXEL_ICC_PROFILE_KIND_GRAY,
    SIXEL_ICC_PROFILE_KIND_CMYK
} sixel_icc_profile_kind_t;

typedef struct sixel_icc_profile {
    sixel_icc_profile_kind_t kind;
    sixel_icc_profile_pcs_t pcs;
    double matrix_to_xyz_d50[3][3];
    double gray_white_xyz_d50[3];
    sixel_icc_curve_t curves[3];
    sixel_icc_lut_t a2b_lut[SIXEL_ICC_A2B_SLOT_COUNT];
    sixel_icc_mab_pipeline_t a2b_mab[SIXEL_ICC_A2B_SLOT_COUNT];
    sixel_icc_lut_t b2a_lut[SIXEL_ICC_B2A_SLOT_COUNT];
    sixel_icc_mab_pipeline_t b2a_mab[SIXEL_ICC_B2A_SLOT_COUNT];
} sixel_icc_profile_t;

SIXEL_INTERNAL_API int
sixel_icc_parse_profile(void const *data,
                        size_t length,
                        sixel_icc_profile_t *out_profile);

SIXEL_INTERNAL_API int
sixel_icc_parse_png_iccp(unsigned char const *png_data,
                         size_t png_size,
                         sixel_icc_profile_t *out_profile);

SIXEL_INTERNAL_API void
sixel_icc_profile_destroy(sixel_icc_profile_t *profile);

#endif
