/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "icc-apply.h"

#if HAVE_MATH_H
#include <math.h>
#endif

#include <stddef.h>

static double const sixel_icc_xyz_d50_to_d65[3][3] = {
    { 0.955576615033105, -0.023039344716079, 0.063163632249801 },
    { -0.028289544243554, 1.009941617371114, 0.021007654996191 },
    { 0.012298165717208, -0.020483025232449, 1.329909826449758 }
};

static double const sixel_icc_xyz_to_srgb_d65[3][3] = {
    { 3.240969941904521, -1.537383177570093, -0.498610760293003 },
    { -0.969243636280880, 1.875967501507721, 0.041555057407176 },
    { 0.055630079696993, -0.203976958888977, 1.056971514242878 }
};

static double
sixel_icc_clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static double
sixel_icc_encode_srgb_unit(double linear_value)
{
    linear_value = sixel_icc_clamp_unit(linear_value);
    if (linear_value <= 0.0031308) {
        return linear_value * 12.92;
    }
    return 1.055 * pow(linear_value, 1.0 / 2.4) - 0.055;
}

static double
sixel_icc_eval_curve(sixel_icc_curve_t const *curve, double value)
{
    size_t lower_index;
    size_t upper_index;
    double position;
    double lower_value;
    double upper_value;
    double fraction;

    value = sixel_icc_clamp_unit(value);
    if (curve == NULL) {
        return value;
    }

    switch (curve->kind) {
    case SIXEL_ICC_CURVE_IDENTITY:
        return value;
    case SIXEL_ICC_CURVE_GAMMA:
        if (curve->gamma <= 0.0) {
            return value;
        }
        return sixel_icc_clamp_unit(pow(value, curve->gamma));
    case SIXEL_ICC_CURVE_TABLE:
        if (curve->table == NULL || curve->table_length == 0u) {
            return value;
        }
        if (curve->table_length == 1u) {
            return (double)curve->table[0] / 65535.0;
        }

        position = value * (double)(curve->table_length - 1u);
        lower_index = (size_t)position;
        if (lower_index >= curve->table_length - 1u) {
            return (double)curve->table[curve->table_length - 1u] / 65535.0;
        }

        upper_index = lower_index + 1u;
        fraction = position - (double)lower_index;
        lower_value = (double)curve->table[lower_index] / 65535.0;
        upper_value = (double)curve->table[upper_index] / 65535.0;
        return sixel_icc_clamp_unit(lower_value + (upper_value - lower_value) * fraction);
    default:
        break;
    }

    return value;
}

static void
sixel_icc_apply_matrix(double const matrix[3][3],
                       double const in[3],
                       double out[3])
{
    out[0] = matrix[0][0] * in[0] + matrix[0][1] * in[1] + matrix[0][2] * in[2];
    out[1] = matrix[1][0] * in[0] + matrix[1][1] * in[1] + matrix[1][2] * in[2];
    out[2] = matrix[2][0] * in[0] + matrix[2][1] * in[1] + matrix[2][2] * in[2];
}

static int
sixel_icc_apply_rgb_triplet_internal(double rgb[3],
                                     sixel_icc_profile_t const *profile)
{
    double source_linear[3];
    double xyz_d50[3];
    double xyz_d65[3];
    double srgb_linear[3];

    if (rgb == NULL || profile == NULL) {
        return 0;
    }
    if (profile->kind == SIXEL_ICC_PROFILE_KIND_INVALID) {
        return 0;
    }

    if (profile->kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        source_linear[0] = sixel_icc_eval_curve(&profile->curves[0], rgb[0]);
        source_linear[1] = sixel_icc_eval_curve(&profile->curves[1], rgb[1]);
        source_linear[2] = sixel_icc_eval_curve(&profile->curves[2], rgb[2]);

        sixel_icc_apply_matrix(profile->matrix_to_xyz_d50, source_linear, xyz_d50);
    } else if (profile->kind == SIXEL_ICC_PROFILE_KIND_GRAY) {
        double gray_linear;

        gray_linear = sixel_icc_eval_curve(&profile->curves[0], rgb[0]);
        xyz_d50[0] = gray_linear * profile->gray_white_xyz_d50[0];
        xyz_d50[1] = gray_linear * profile->gray_white_xyz_d50[1];
        xyz_d50[2] = gray_linear * profile->gray_white_xyz_d50[2];
    } else {
        return 0;
    }

    sixel_icc_apply_matrix(sixel_icc_xyz_d50_to_d65, xyz_d50, xyz_d65);
    sixel_icc_apply_matrix(sixel_icc_xyz_to_srgb_d65, xyz_d65, srgb_linear);

    rgb[0] = sixel_icc_encode_srgb_unit(srgb_linear[0]);
    rgb[1] = sixel_icc_encode_srgb_unit(srgb_linear[1]);
    rgb[2] = sixel_icc_encode_srgb_unit(srgb_linear[2]);

    return 1;
}

int
sixel_icc_apply_rgb_u8(unsigned char *pixels,
                       size_t pixel_count,
                       sixel_icc_profile_t const *profile)
{
    size_t i;

    if (pixels == NULL || pixel_count == 0u || profile == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        size_t offset;
        double rgb[3];

        offset = i * 3u;
        rgb[0] = (double)pixels[offset + 0u] / 255.0;
        rgb[1] = (double)pixels[offset + 1u] / 255.0;
        rgb[2] = (double)pixels[offset + 2u] / 255.0;

        if (!sixel_icc_apply_rgb_triplet_internal(rgb, profile)) {
            return 0;
        }

        pixels[offset + 0u] = (unsigned char)(sixel_icc_clamp_unit(rgb[0]) * 255.0 + 0.5);
        pixels[offset + 1u] = (unsigned char)(sixel_icc_clamp_unit(rgb[1]) * 255.0 + 0.5);
        pixels[offset + 2u] = (unsigned char)(sixel_icc_clamp_unit(rgb[2]) * 255.0 + 0.5);
    }

    return 1;
}

int
sixel_icc_apply_rgb_float32(float *pixels,
                            size_t pixel_count,
                            sixel_icc_profile_t const *profile)
{
    size_t i;

    if (pixels == NULL || pixel_count == 0u || profile == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        size_t offset;
        double rgb[3];

        offset = i * 3u;
        rgb[0] = (double)pixels[offset + 0u];
        rgb[1] = (double)pixels[offset + 1u];
        rgb[2] = (double)pixels[offset + 2u];

        if (!sixel_icc_apply_rgb_triplet_internal(rgb, profile)) {
            return 0;
        }

        pixels[offset + 0u] = (float)sixel_icc_clamp_unit(rgb[0]);
        pixels[offset + 1u] = (float)sixel_icc_clamp_unit(rgb[1]);
        pixels[offset + 2u] = (float)sixel_icc_clamp_unit(rgb[2]);
    }

    return 1;
}

int
sixel_icc_apply_gray_u8(unsigned char *pixels,
                        size_t pixel_count,
                        sixel_icc_profile_t const *profile)
{
    size_t i;

    if (pixels == NULL || pixel_count == 0u || profile == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        double rgb[3];
        double v;

        v = (double)pixels[i] / 255.0;
        rgb[0] = v;
        rgb[1] = v;
        rgb[2] = v;

        if (!sixel_icc_apply_rgb_triplet_internal(rgb, profile)) {
            return 0;
        }

        pixels[i] = (unsigned char)(sixel_icc_clamp_unit(rgb[0]) * 255.0 + 0.5);
    }

    return 1;
}

int
sixel_icc_apply_rgb_triplet_unit(double rgb[3],
                                 sixel_icc_profile_t const *profile)
{
    return sixel_icc_apply_rgb_triplet_internal(rgb, profile);
}
