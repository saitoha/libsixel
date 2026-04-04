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

static double const sixel_icc_d50_white_xyz[3] = {
    0.9642, 1.0, 0.8249
};

static void
sixel_icc_apply_matrix(double const matrix[3][3],
                       double const in[3],
                       double out[3]);

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
    case SIXEL_ICC_CURVE_SEGM_TABLE:
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

static double
sixel_icc_eval_lut_table(uint16_t const *table,
                         size_t table_length,
                         double value)
{
    size_t lower_index;
    size_t upper_index;
    double position;
    double fraction;
    double lower_value;
    double upper_value;

    if (table == NULL || table_length == 0u) {
        return sixel_icc_clamp_unit(value);
    }
    if (table_length == 1u) {
        return (double)table[0] / 65535.0;
    }

    value = sixel_icc_clamp_unit(value);
    position = value * (double)(table_length - 1u);
    lower_index = (size_t)position;
    if (lower_index >= table_length - 1u) {
        return (double)table[table_length - 1u] / 65535.0;
    }

    upper_index = lower_index + 1u;
    fraction = position - (double)lower_index;
    lower_value = (double)table[lower_index] / 65535.0;
    upper_value = (double)table[upper_index] / 65535.0;
    return sixel_icc_clamp_unit(lower_value + (upper_value - lower_value) * fraction);
}

static int
sixel_icc_eval_clut(sixel_icc_lut_t const *lut,
                    double const *inputs,
                    double *outputs)
{
    uint8_t input_channels;
    uint8_t output_channels;
    size_t corner_count;
    size_t channel_index;
    size_t output_index;
    size_t corner;
    size_t grid_points;
    size_t lower[16];
    double fraction[16];

    if (lut == NULL || inputs == NULL || outputs == NULL) {
        return 0;
    }
    input_channels = lut->input_channels;
    output_channels = lut->output_channels;
    if (input_channels == 0u || output_channels == 0u ||
        input_channels > 16u || output_channels > 16u ||
        lut->clut_values == NULL || lut->clut_grid_points == 0u) {
        return 0;
    }
    if (input_channels >= sizeof(size_t) * 8u) {
        return 0;
    }

    grid_points = (size_t)lut->clut_grid_points;
    for (channel_index = 0u; channel_index < (size_t)input_channels; ++channel_index) {
        double v;
        double position;
        size_t low;

        v = sixel_icc_clamp_unit(inputs[channel_index]);
        if (grid_points <= 1u) {
            lower[channel_index] = 0u;
            fraction[channel_index] = 0.0;
            continue;
        }

        position = v * (double)(grid_points - 1u);
        low = (size_t)position;
        if (low >= grid_points - 1u) {
            low = grid_points - 2u;
            fraction[channel_index] = 1.0;
        } else {
            fraction[channel_index] = position - (double)low;
        }
        lower[channel_index] = low;
    }

    for (output_index = 0u; output_index < (size_t)output_channels; ++output_index) {
        outputs[output_index] = 0.0;
    }

    corner_count = (size_t)1u << input_channels;
    for (corner = 0u; corner < corner_count; ++corner) {
        double weight;
        size_t clut_index;

        weight = 1.0;
        clut_index = 0u;

        for (channel_index = 0u; channel_index < (size_t)input_channels; ++channel_index) {
            size_t coord;
            double f;
            int use_high;

            f = fraction[channel_index];
            use_high = ((corner >> channel_index) & 1u) != 0u;
            if (use_high) {
                coord = lower[channel_index] + ((grid_points <= 1u) ? 0u : 1u);
                weight *= f;
            } else {
                coord = lower[channel_index];
                weight *= (1.0 - f);
            }
            clut_index = clut_index * grid_points + coord;
        }
        if (weight == 0.0) {
            continue;
        }

        clut_index *= (size_t)output_channels;
        for (output_index = 0u; output_index < (size_t)output_channels; ++output_index) {
            outputs[output_index] += weight
                * ((double)lut->clut_values[clut_index + output_index] / 65535.0);
        }
    }

    for (output_index = 0u; output_index < (size_t)output_channels; ++output_index) {
        outputs[output_index] = sixel_icc_clamp_unit(outputs[output_index]);
    }

    return 1;
}

static double
sixel_icc_lab_inverse_f(double t)
{
    double delta;

    delta = 6.0 / 29.0;
    if (t > delta) {
        return t * t * t;
    }
    return 3.0 * delta * delta * (t - 4.0 / 29.0);
}

static void
sixel_icc_decode_lab_unit(double const in[3], double lab[3])
{
    lab[0] = sixel_icc_clamp_unit(in[0]) * 100.0;
    lab[1] = sixel_icc_clamp_unit(in[1]) * 255.0 - 128.0;
    lab[2] = sixel_icc_clamp_unit(in[2]) * 255.0 - 128.0;
}

static void
sixel_icc_lab_to_xyz_d50(double const lab[3], double xyz[3])
{
    double fy;
    double fx;
    double fz;

    fy = (lab[0] + 16.0) / 116.0;
    fx = fy + lab[1] / 500.0;
    fz = fy - lab[2] / 200.0;

    xyz[0] = sixel_icc_d50_white_xyz[0] * sixel_icc_lab_inverse_f(fx);
    xyz[1] = sixel_icc_d50_white_xyz[1] * sixel_icc_lab_inverse_f(fy);
    xyz[2] = sixel_icc_d50_white_xyz[2] * sixel_icc_lab_inverse_f(fz);
}

static int
sixel_icc_decode_pcs_unit_to_xyz_d50(double xyz_d50[3],
                                     double const pcs_unit[3],
                                     sixel_icc_profile_pcs_t pcs)
{
    double lab[3];

    if (xyz_d50 == NULL || pcs_unit == NULL) {
        return 0;
    }

    if (pcs == SIXEL_ICC_PROFILE_PCS_XYZ) {
        xyz_d50[0] = pcs_unit[0];
        xyz_d50[1] = pcs_unit[1];
        xyz_d50[2] = pcs_unit[2];
        return 1;
    }
    if (pcs == SIXEL_ICC_PROFILE_PCS_LAB) {
        sixel_icc_decode_lab_unit(pcs_unit, lab);
        sixel_icc_lab_to_xyz_d50(lab, xyz_d50);
        return 1;
    }

    return 0;
}

static int
sixel_icc_apply_a2b0_lut_to_xyz_d50(double xyz_d50[3],
                                    double const *inputs,
                                    size_t input_channel_count,
                                    sixel_icc_profile_t const *profile)
{
    double lut_inputs[4];
    double pcs_unit[3];
    size_t channel;
    uint16_t in_entries;
    uint16_t out_entries;
    sixel_icc_lut_t const *lut;

    channel = 0u;
    in_entries = 0u;
    out_entries = 0u;
    lut = NULL;

    if (xyz_d50 == NULL || inputs == NULL || profile == NULL) {
        return 0;
    }
    if (input_channel_count == 0u || input_channel_count > 4u) {
        return 0;
    }

    lut = &profile->a2b0_lut;
    if (lut->kind == SIXEL_ICC_LUT_INVALID ||
        lut->input_tables == NULL ||
        lut->clut_values == NULL ||
        lut->output_tables == NULL ||
        lut->input_channels != input_channel_count ||
        lut->output_channels != 3u) {
        return 0;
    }

    in_entries = lut->input_entries;
    out_entries = lut->output_entries;
    if (in_entries == 0u || out_entries == 0u) {
        return 0;
    }

    for (channel = 0u; channel < input_channel_count; ++channel) {
        uint16_t const *table;

        table = lut->input_tables + channel * (size_t)in_entries;
        lut_inputs[channel] = sixel_icc_eval_lut_table(table,
                                                       (size_t)in_entries,
                                                       inputs[channel]);
    }

    if (!sixel_icc_eval_clut(lut, lut_inputs, pcs_unit)) {
        return 0;
    }

    for (channel = 0u; channel < 3u; ++channel) {
        uint16_t const *table;

        table = lut->output_tables + channel * (size_t)out_entries;
        pcs_unit[channel] = sixel_icc_eval_lut_table(table,
                                                     (size_t)out_entries,
                                                     pcs_unit[channel]);
    }

    return sixel_icc_decode_pcs_unit_to_xyz_d50(xyz_d50,
                                                pcs_unit,
                                                profile->pcs);
}

static int
sixel_icc_apply_rgb_gray_triplet_lut(double rgb[3],
                                     sixel_icc_profile_t const *profile)
{
    double inputs[3];
    double xyz_d50[3];
    double xyz_d65[3];
    double srgb_linear[3];
    size_t input_channel_count;

    input_channel_count = 0u;

    if (rgb == NULL || profile == NULL) {
        return 0;
    }

    if (profile->kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        inputs[0] = rgb[0];
        inputs[1] = rgb[1];
        inputs[2] = rgb[2];
        input_channel_count = 3u;
    } else if (profile->kind == SIXEL_ICC_PROFILE_KIND_GRAY) {
        inputs[0] = rgb[0];
        input_channel_count = 1u;
    } else {
        return 0;
    }

    if (!sixel_icc_apply_a2b0_lut_to_xyz_d50(xyz_d50,
                                             inputs,
                                             input_channel_count,
                                             profile)) {
        return 0;
    }

    sixel_icc_apply_matrix(sixel_icc_xyz_d50_to_d65, xyz_d50, xyz_d65);
    sixel_icc_apply_matrix(sixel_icc_xyz_to_srgb_d65, xyz_d65, srgb_linear);

    rgb[0] = sixel_icc_encode_srgb_unit(srgb_linear[0]);
    rgb[1] = sixel_icc_encode_srgb_unit(srgb_linear[1]);
    rgb[2] = sixel_icc_encode_srgb_unit(srgb_linear[2]);
    return 1;
}

static int
sixel_icc_apply_cmyk_triplet_internal(double rgb[3],
                                      double const cmyk[4],
                                      sixel_icc_profile_t const *profile)
{
    double xyz_d50[3];
    double xyz_d65[3];
    double srgb_linear[3];

    if (rgb == NULL || cmyk == NULL || profile == NULL) {
        return 0;
    }
    if (profile->kind != SIXEL_ICC_PROFILE_KIND_CMYK) {
        return 0;
    }
    if (!sixel_icc_apply_a2b0_lut_to_xyz_d50(xyz_d50,
                                             cmyk,
                                             4u,
                                             profile)) {
        return 0;
    }

    sixel_icc_apply_matrix(sixel_icc_xyz_d50_to_d65, xyz_d50, xyz_d65);
    sixel_icc_apply_matrix(sixel_icc_xyz_to_srgb_d65, xyz_d65, srgb_linear);

    rgb[0] = sixel_icc_encode_srgb_unit(srgb_linear[0]);
    rgb[1] = sixel_icc_encode_srgb_unit(srgb_linear[1]);
    rgb[2] = sixel_icc_encode_srgb_unit(srgb_linear[2]);
    return 1;
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

    if (profile->a2b0_lut.kind != SIXEL_ICC_LUT_INVALID &&
        (profile->kind == SIXEL_ICC_PROFILE_KIND_RGB ||
         profile->kind == SIXEL_ICC_PROFILE_KIND_GRAY)) {
        if (sixel_icc_apply_rgb_gray_triplet_lut(rgb, profile)) {
            return 1;
        }
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

int
sixel_icc_apply_cmyk_u8_to_rgb_float32(float *dst_pixels,
                                       unsigned char const *src_pixels,
                                       size_t pixel_count,
                                       sixel_icc_profile_t const *profile)
{
    size_t i;

    if (dst_pixels == NULL || src_pixels == NULL ||
        pixel_count == 0u || profile == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        size_t src_offset;
        size_t dst_offset;
        double cmyk[4];
        double rgb[3];

        src_offset = i * 4u;
        dst_offset = i * 3u;
        cmyk[0] = (double)src_pixels[src_offset + 0u] / 255.0;
        cmyk[1] = (double)src_pixels[src_offset + 1u] / 255.0;
        cmyk[2] = (double)src_pixels[src_offset + 2u] / 255.0;
        cmyk[3] = (double)src_pixels[src_offset + 3u] / 255.0;

        if (!sixel_icc_apply_cmyk_triplet_internal(rgb, cmyk, profile)) {
            return 0;
        }

        dst_pixels[dst_offset + 0u] = (float)sixel_icc_clamp_unit(rgb[0]);
        dst_pixels[dst_offset + 1u] = (float)sixel_icc_clamp_unit(rgb[1]);
        dst_pixels[dst_offset + 2u] = (float)sixel_icc_clamp_unit(rgb[2]);
    }

    return 1;
}

int
sixel_icc_apply_cmyk_u16_to_rgb_float32(float *dst_pixels,
                                        uint16_t const *src_pixels,
                                        size_t pixel_count,
                                        sixel_icc_profile_t const *profile)
{
    size_t i;

    if (dst_pixels == NULL || src_pixels == NULL ||
        pixel_count == 0u || profile == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        size_t src_offset;
        size_t dst_offset;
        double cmyk[4];
        double rgb[3];

        src_offset = i * 4u;
        dst_offset = i * 3u;
        cmyk[0] = (double)src_pixels[src_offset + 0u] / 65535.0;
        cmyk[1] = (double)src_pixels[src_offset + 1u] / 65535.0;
        cmyk[2] = (double)src_pixels[src_offset + 2u] / 65535.0;
        cmyk[3] = (double)src_pixels[src_offset + 3u] / 65535.0;

        if (!sixel_icc_apply_cmyk_triplet_internal(rgb, cmyk, profile)) {
            return 0;
        }

        dst_pixels[dst_offset + 0u] = (float)sixel_icc_clamp_unit(rgb[0]);
        dst_pixels[dst_offset + 1u] = (float)sixel_icc_clamp_unit(rgb[1]);
        dst_pixels[dst_offset + 2u] = (float)sixel_icc_clamp_unit(rgb[2]);
    }

    return 1;
}

int
sixel_icc_apply_cmyk_float32_to_rgb_float32(float *dst_pixels,
                                            float const *src_pixels,
                                            size_t pixel_count,
                                            sixel_icc_profile_t const *profile)
{
    size_t i;

    if (dst_pixels == NULL || src_pixels == NULL ||
        pixel_count == 0u || profile == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        size_t src_offset;
        size_t dst_offset;
        double cmyk[4];
        double rgb[3];

        src_offset = i * 4u;
        dst_offset = i * 3u;
        cmyk[0] = sixel_icc_clamp_unit((double)src_pixels[src_offset + 0u]);
        cmyk[1] = sixel_icc_clamp_unit((double)src_pixels[src_offset + 1u]);
        cmyk[2] = sixel_icc_clamp_unit((double)src_pixels[src_offset + 2u]);
        cmyk[3] = sixel_icc_clamp_unit((double)src_pixels[src_offset + 3u]);

        if (!sixel_icc_apply_cmyk_triplet_internal(rgb, cmyk, profile)) {
            return 0;
        }

        dst_pixels[dst_offset + 0u] = (float)sixel_icc_clamp_unit(rgb[0]);
        dst_pixels[dst_offset + 1u] = (float)sixel_icc_clamp_unit(rgb[1]);
        dst_pixels[dst_offset + 2u] = (float)sixel_icc_clamp_unit(rgb[2]);
    }

    return 1;
}
