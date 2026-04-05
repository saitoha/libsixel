/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "icc-parse.h"

#if HAVE_MATH_H
#include <math.h>
#endif

#include <stdlib.h>
#include <string.h>

char *stbi_zlib_decode_malloc_guesssize_headerflag(char const *buffer,
                                                   int len,
                                                   int initial_size,
                                                   int *outlen,
                                                   int parse_header);
void stbi_image_free(void *retval_from_stbi_load);

#define SIXEL_ICC_PARAMETRIC_TABLE_SIZE 4096u

static uint16_t
sixel_icc_read_be16(unsigned char const *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8u) | (uint16_t)p[1]);
}

static uint32_t
sixel_icc_read_be32(unsigned char const *p)
{
    return ((uint32_t)p[0] << 24u)
        | ((uint32_t)p[1] << 16u)
        | ((uint32_t)p[2] << 8u)
        | (uint32_t)p[3];
}

static double
sixel_icc_read_s15fixed16(unsigned char const *p)
{
    int32_t fixed;

    fixed = (int32_t)sixel_icc_read_be32(p);
    return (double)fixed / 65536.0;
}

static void
sixel_icc_curve_reset(sixel_icc_curve_t *curve)
{
    if (curve == NULL) {
        return;
    }

    curve->kind = SIXEL_ICC_CURVE_INVALID;
    curve->gamma = 1.0;
    curve->table = NULL;
    curve->table_length = 0u;
}

static void
sixel_icc_lut_reset(sixel_icc_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    lut->kind = SIXEL_ICC_LUT_INVALID;
    lut->input_channels = 0u;
    lut->output_channels = 0u;
    lut->clut_grid_points = 0u;
    lut->input_entries = 0u;
    lut->output_entries = 0u;
    lut->input_tables = NULL;
    lut->clut_values = NULL;
    lut->output_tables = NULL;
}

static void
sixel_icc_lut_destroy(sixel_icc_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->input_tables != NULL) {
        free(lut->input_tables);
    }
    if (lut->clut_values != NULL) {
        free(lut->clut_values);
    }
    if (lut->output_tables != NULL) {
        free(lut->output_tables);
    }
    sixel_icc_lut_reset(lut);
}

static void
sixel_icc_profile_reset(sixel_icc_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }

    memset(profile, 0, sizeof(*profile));
    profile->kind = SIXEL_ICC_PROFILE_KIND_INVALID;
    profile->pcs = SIXEL_ICC_PROFILE_PCS_INVALID;
    sixel_icc_curve_reset(&profile->curves[0]);
    sixel_icc_curve_reset(&profile->curves[1]);
    sixel_icc_curve_reset(&profile->curves[2]);
    sixel_icc_lut_reset(&profile->a2b0_lut);
}

void
sixel_icc_profile_destroy(sixel_icc_profile_t *profile)
{
    size_t i;

    if (profile == NULL) {
        return;
    }

    for (i = 0u; i < 3u; ++i) {
        if (profile->curves[i].table != NULL) {
            free(profile->curves[i].table);
            profile->curves[i].table = NULL;
            profile->curves[i].table_length = 0u;
        }
        profile->curves[i].kind = SIXEL_ICC_CURVE_INVALID;
        profile->curves[i].gamma = 1.0;
    }
    sixel_icc_lut_destroy(&profile->a2b0_lut);

    profile->kind = SIXEL_ICC_PROFILE_KIND_INVALID;
    profile->pcs = SIXEL_ICC_PROFILE_PCS_INVALID;
    memset(profile->matrix_to_xyz_d50, 0, sizeof(profile->matrix_to_xyz_d50));
    memset(profile->gray_white_xyz_d50, 0, sizeof(profile->gray_white_xyz_d50));
}

static int
sixel_icc_find_tag(unsigned char const *profile_data,
                   size_t profile_size,
                   char const signature[4],
                   size_t *out_offset,
                   size_t *out_length)
{
    uint32_t tag_count;
    size_t table_offset;
    size_t i;

    if (profile_data == NULL || signature == NULL ||
        out_offset == NULL || out_length == NULL) {
        return 0;
    }
    if (profile_size < 132u) {
        return 0;
    }

    tag_count = sixel_icc_read_be32(profile_data + 128u);
    if ((size_t)tag_count > (profile_size - 132u) / 12u) {
        return 0;
    }

    table_offset = 132u;
    for (i = 0u; i < (size_t)tag_count; ++i) {
        unsigned char const *entry;
        uint32_t tag_offset_u32;
        uint32_t tag_length_u32;
        size_t tag_offset;
        size_t tag_length;

        entry = profile_data + table_offset + i * 12u;
        if (memcmp(entry + 0u, signature, 4u) != 0) {
            continue;
        }

        tag_offset_u32 = sixel_icc_read_be32(entry + 4u);
        tag_length_u32 = sixel_icc_read_be32(entry + 8u);
        tag_offset = (size_t)tag_offset_u32;
        tag_length = (size_t)tag_length_u32;

        if (tag_length == 0u || tag_offset > profile_size ||
            tag_length > profile_size - tag_offset) {
            return 0;
        }

        *out_offset = tag_offset;
        *out_length = tag_length;
        return 1;
    }

    return 0;
}

static int
sixel_icc_parse_xyz_tag(unsigned char const *profile_data,
                        size_t profile_size,
                        size_t tag_offset,
                        size_t tag_length,
                        double xyz[3])
{
    unsigned char const *tag_data;

    if (profile_data == NULL || xyz == NULL) {
        return 0;
    }
    if (tag_offset > profile_size || tag_length > profile_size - tag_offset) {
        return 0;
    }
    if (tag_length < 20u) {
        return 0;
    }

    tag_data = profile_data + tag_offset;
    if (memcmp(tag_data + 0u, "XYZ ", 4u) != 0) {
        return 0;
    }

    xyz[0] = sixel_icc_read_s15fixed16(tag_data + 8u);
    xyz[1] = sixel_icc_read_s15fixed16(tag_data + 12u);
    xyz[2] = sixel_icc_read_s15fixed16(tag_data + 16u);

    return 1;
}

static double
sixel_icc_parse_clamp_unit(double value)
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
sixel_icc_eval_parametric(uint16_t function_type,
                          double const *params,
                          size_t param_count,
                          double x,
                          int *ok)
{
    double y;

    y = 0.0;
    if (ok != NULL) {
        *ok = 1;
    }
    x = sixel_icc_parse_clamp_unit(x);

    switch (function_type) {
    case 0u:
        if (param_count < 1u || params[0] <= 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        y = pow(x, params[0]);
        break;
    case 1u:
        if (param_count < 3u || params[0] <= 0.0 || params[1] == 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        if (x >= (-params[2] / params[1])) {
            y = pow(params[1] * x + params[2], params[0]);
        } else {
            y = 0.0;
        }
        break;
    case 2u:
        if (param_count < 4u || params[0] <= 0.0 || params[1] == 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        if (x >= (-params[2] / params[1])) {
            y = pow(params[1] * x + params[2], params[0]) + params[3];
        } else {
            y = params[3];
        }
        break;
    case 3u:
        if (param_count < 5u || params[0] <= 0.0 || params[1] <= 0.0 ||
            params[3] < 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        if (x >= params[4]) {
            y = pow(params[1] * x + params[2], params[0]);
        } else {
            y = params[3] * x;
        }
        break;
    case 4u:
        if (param_count < 7u || params[0] <= 0.0 || params[1] <= 0.0 ||
            params[3] < 0.0 || params[4] < 0.0) {
            if (ok != NULL) {
                *ok = 0;
            }
            return 0.0;
        }
        if (x >= params[5]) {
            y = pow(params[1] * x + params[2], params[0]) + params[4];
        } else {
            y = params[3] * x + params[6];
        }
        break;
    default:
        if (ok != NULL) {
            *ok = 0;
        }
        return 0.0;
    }

    return sixel_icc_parse_clamp_unit(y);
}

typedef struct sixel_icc_segm_segment {
    uint16_t function_type;
    size_t param_count;
    double params[7];
} sixel_icc_segm_segment_t;

static int
sixel_icc_parse_segm_param_count(uint16_t function_type, size_t *param_count)
{
    if (param_count == NULL) {
        return 0;
    }

    switch (function_type) {
    case 0u:
        *param_count = 1u;
        return 1;
    case 1u:
        *param_count = 3u;
        return 1;
    case 2u:
        *param_count = 4u;
        return 1;
    case 3u:
        *param_count = 5u;
        return 1;
    case 4u:
        *param_count = 7u;
        return 1;
    default:
        break;
    }

    return 0;
}

static int
sixel_icc_parse_segmented_curve_tag(unsigned char const *tag_data,
                                    size_t tag_length,
                                    sixel_icc_curve_t *out_curve)
{
    uint32_t segment_count_u32;
    size_t segment_count;
    size_t breakpoint_count;
    double *breakpoints;
    sixel_icc_segm_segment_t *segments;
    size_t offset;
    size_t i;
    size_t j;
    size_t sample_count;
    size_t segment_index;
    size_t param_count;
    uint16_t function_type;
    double x;
    double y;
    int ok;

    segment_count_u32 = 0u;
    segment_count = 0u;
    breakpoint_count = 0u;
    breakpoints = NULL;
    segments = NULL;
    offset = 0u;
    i = 0u;
    j = 0u;
    sample_count = 0u;
    segment_index = 0u;
    param_count = 0u;
    function_type = 0u;
    x = 0.0;
    y = 0.0;
    ok = 0;

    if (tag_data == NULL || out_curve == NULL || tag_length < 12u) {
        return 0;
    }
    if (memcmp(tag_data + 0u, "segm", 4u) != 0) {
        return 0;
    }

    segment_count_u32 = sixel_icc_read_be32(tag_data + 8u);
    segment_count = (size_t)segment_count_u32;
    if (segment_count == 0u) {
        return 0;
    }

    if (segment_count > SIZE_MAX / sizeof(*segments)) {
        return 0;
    }
    breakpoint_count = segment_count - 1u;
    if (breakpoint_count > 0u) {
        if (breakpoint_count > (tag_length - 12u) / 4u) {
            return 0;
        }
        if (breakpoint_count > SIZE_MAX / sizeof(*breakpoints)) {
            return 0;
        }
        breakpoints = (double *)malloc(breakpoint_count * sizeof(*breakpoints));
        if (breakpoints == NULL) {
            return 0;
        }
    }

    segments = (sixel_icc_segm_segment_t *)malloc(segment_count
                                                  * sizeof(*segments));
    if (segments == NULL) {
        free(breakpoints);
        return 0;
    }
    memset(segments, 0, segment_count * sizeof(*segments));

    offset = 12u;
    for (i = 0u; i < breakpoint_count; ++i) {
        breakpoints[i] = sixel_icc_read_s15fixed16(tag_data + offset);
        if (i > 0u && breakpoints[i] < breakpoints[i - 1u]) {
            free(segments);
            free(breakpoints);
            return 0;
        }
        offset += 4u;
    }

    for (i = 0u; i < segment_count; ++i) {
        param_count = 0u;
        if (tag_length - offset < 4u) {
            free(segments);
            free(breakpoints);
            return 0;
        }
        function_type = sixel_icc_read_be16(tag_data + offset);
        if (!sixel_icc_parse_segm_param_count(function_type,
                                              &param_count)) {
            free(segments);
            free(breakpoints);
            return 0;
        }
        if (param_count > 7u || param_count > (tag_length - offset - 4u) / 4u) {
            free(segments);
            free(breakpoints);
            return 0;
        }

        segments[i].function_type = function_type;
        segments[i].param_count = param_count;
        for (j = 0u; j < param_count; ++j) {
            segments[i].params[j] = sixel_icc_read_s15fixed16(
                tag_data + offset + 4u + j * 4u);
        }
        offset += 4u + param_count * 4u;
    }

    sample_count = SIXEL_ICC_PARAMETRIC_TABLE_SIZE;
    out_curve->table = (uint16_t *)malloc(sample_count * sizeof(uint16_t));
    if (out_curve->table == NULL) {
        free(segments);
        free(breakpoints);
        return 0;
    }
    out_curve->table_length = sample_count;
    out_curve->kind = SIXEL_ICC_CURVE_SEGM_TABLE;

    for (i = 0u; i < sample_count; ++i) {
        x = (sample_count <= 1u)
            ? 0.0
            : (double)i / (double)(sample_count - 1u);
        segment_index = 0u;
        if (breakpoint_count > 0u) {
            while (segment_index < breakpoint_count &&
                   x > breakpoints[segment_index]) {
                ++segment_index;
            }
        }
        if (segment_index >= segment_count) {
            free(out_curve->table);
            out_curve->table = NULL;
            out_curve->table_length = 0u;
            out_curve->kind = SIXEL_ICC_CURVE_INVALID;
            free(segments);
            free(breakpoints);
            return 0;
        }

        y = sixel_icc_eval_parametric(segments[segment_index].function_type,
                                      segments[segment_index].params,
                                      segments[segment_index].param_count,
                                      x,
                                      &ok);
        if (!ok) {
            free(out_curve->table);
            out_curve->table = NULL;
            out_curve->table_length = 0u;
            out_curve->kind = SIXEL_ICC_CURVE_INVALID;
            free(segments);
            free(breakpoints);
            return 0;
        }

        out_curve->table[i] = (uint16_t)(
            sixel_icc_parse_clamp_unit(y) * 65535.0 + 0.5);
    }

    free(segments);
    free(breakpoints);
    return 1;
}

static int
sixel_icc_parse_curve_tag(unsigned char const *profile_data,
                          size_t profile_size,
                          size_t tag_offset,
                          size_t tag_length,
                          sixel_icc_curve_t *out_curve)
{
    unsigned char const *tag_data;

    if (profile_data == NULL || out_curve == NULL) {
        return 0;
    }
    if (tag_offset > profile_size || tag_length > profile_size - tag_offset) {
        return 0;
    }
    if (tag_length < 12u) {
        return 0;
    }

    tag_data = profile_data + tag_offset;

    if (memcmp(tag_data + 0u, "curv", 4u) == 0) {
        uint32_t count;
        size_t table_length;
        size_t i;

        count = sixel_icc_read_be32(tag_data + 8u);
        if (count == 0u) {
            out_curve->kind = SIXEL_ICC_CURVE_IDENTITY;
            out_curve->gamma = 1.0;
            return 1;
        }
        if (count == 1u) {
            uint16_t gamma_u8f8;

            if (tag_length < 14u) {
                return 0;
            }
            gamma_u8f8 = sixel_icc_read_be16(tag_data + 12u);
            out_curve->kind = SIXEL_ICC_CURVE_GAMMA;
            out_curve->gamma = (double)gamma_u8f8 / 256.0;
            if (out_curve->gamma <= 0.0) {
                out_curve->gamma = 1.0;
            }
            return 1;
        }

#if SIZE_MAX <= UINT32_MAX
        if ((size_t)count > (SIZE_MAX - 12u) / 2u) {
            return 0;
        }
#endif
        if (tag_length < 12u + (size_t)count * 2u) {
            return 0;
        }
#if SIZE_MAX <= UINT32_MAX
        if ((size_t)count > SIZE_MAX / sizeof(uint16_t)) {
            return 0;
        }
#endif
        table_length = (size_t)count;

        out_curve->table = (uint16_t *)malloc(table_length * sizeof(uint16_t));
        if (out_curve->table == NULL) {
            return 0;
        }
        out_curve->table_length = table_length;
        out_curve->kind = SIXEL_ICC_CURVE_TABLE;
        for (i = 0u; i < table_length; ++i) {
            out_curve->table[i] = sixel_icc_read_be16(tag_data + 12u + i * 2u);
        }
        return 1;
    }

    if (memcmp(tag_data + 0u, "para", 4u) == 0) {
        uint16_t function_type;
        size_t param_count;
        double params[7];
        size_t sample_count;
        size_t i;

        function_type = sixel_icc_read_be16(tag_data + 8u);
        switch (function_type) {
        case 0u:
            param_count = 1u;
            break;
        case 1u:
            param_count = 3u;
            break;
        case 2u:
            param_count = 4u;
            break;
        case 3u:
            param_count = 5u;
            break;
        case 4u:
            param_count = 7u;
            break;
        default:
            return 0;
        }
        if (tag_length < 12u + param_count * 4u) {
            return 0;
        }

        memset(params, 0, sizeof(params));
        for (i = 0u; i < param_count; ++i) {
            params[i] = sixel_icc_read_s15fixed16(tag_data + 12u + i * 4u);
        }

        sample_count = SIXEL_ICC_PARAMETRIC_TABLE_SIZE;
        out_curve->table = (uint16_t *)malloc(sample_count * sizeof(uint16_t));
        if (out_curve->table == NULL) {
            return 0;
        }
        out_curve->table_length = sample_count;
        out_curve->kind = SIXEL_ICC_CURVE_TABLE;

        for (i = 0u; i < sample_count; ++i) {
            double x;
            double y;
            int ok;

            x = (sample_count <= 1u)
                ? 0.0
                : (double)i / (double)(sample_count - 1u);
            y = sixel_icc_eval_parametric(function_type,
                                          params,
                                          param_count,
                                          x,
                                          &ok);
            if (!ok) {
                free(out_curve->table);
                out_curve->table = NULL;
                out_curve->table_length = 0u;
                out_curve->kind = SIXEL_ICC_CURVE_INVALID;
                return 0;
            }

            out_curve->table[i] = (uint16_t)(sixel_icc_parse_clamp_unit(y)
                                             * 65535.0 + 0.5);
        }
        return 1;
    }
    if (memcmp(tag_data + 0u, "segm", 4u) == 0) {
        return sixel_icc_parse_segmented_curve_tag(tag_data,
                                                   tag_length,
                                                   out_curve);
    }

    return 0;
}

static int
sixel_icc_size_add(size_t a, size_t b, size_t *out)
{
    if (out == NULL) {
        return 0;
    }
    if (a > SIZE_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static int
sixel_icc_size_multiply(size_t a, size_t b, size_t *out)
{
    if (out == NULL) {
        return 0;
    }
    if (a != 0u && b > SIZE_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

static int
sixel_icc_size_pow(unsigned int base, unsigned int exponent, size_t *out)
{
    size_t value;
    unsigned int i;

    if (out == NULL) {
        return 0;
    }

    value = 1u;
    for (i = 0u; i < exponent; ++i) {
        if (base != 0u && value > SIZE_MAX / (size_t)base) {
            return 0;
        }
        value *= (size_t)base;
    }

    *out = value;
    return 1;
}

static int
sixel_icc_parse_lut8_tag(unsigned char const *tag_data,
                         size_t tag_length,
                         sixel_icc_lut_t *out_lut)
{
    sixel_icc_lut_t parsed;
    unsigned char input_channels;
    unsigned char output_channels;
    unsigned char clut_grid_points;
    uint16_t input_entries;
    uint16_t output_entries;
    size_t input_count;
    size_t clut_points;
    size_t clut_count;
    size_t output_count;
    size_t total_sample_count;
    size_t required_length;
    size_t offset;
    size_t i;

    sixel_icc_lut_reset(&parsed);
    input_channels = 0u;
    output_channels = 0u;
    clut_grid_points = 0u;
    input_entries = 0u;
    output_entries = 0u;
    input_count = 0u;
    clut_points = 0u;
    clut_count = 0u;
    output_count = 0u;
    total_sample_count = 0u;
    required_length = 0u;
    offset = 0u;
    i = 0u;

    if (tag_data == NULL || out_lut == NULL || tag_length < 48u) {
        return 0;
    }
    if (memcmp(tag_data + 0u, "mft1", 4u) != 0) {
        return 0;
    }

    input_channels = tag_data[8u];
    output_channels = tag_data[9u];
    clut_grid_points = tag_data[10u];
    input_entries = 256u;
    output_entries = 256u;

    if (input_channels == 0u || output_channels == 0u || clut_grid_points == 0u) {
        return 0;
    }

    if (!sixel_icc_size_multiply((size_t)input_channels,
                                 (size_t)input_entries,
                                 &input_count) ||
        !sixel_icc_size_pow((unsigned int)clut_grid_points,
                            (unsigned int)input_channels,
                            &clut_points) ||
        !sixel_icc_size_multiply(clut_points,
                                 (size_t)output_channels,
                                 &clut_count) ||
        !sixel_icc_size_multiply((size_t)output_channels,
                                 (size_t)output_entries,
                                 &output_count) ||
        !sixel_icc_size_add(input_count, clut_count, &total_sample_count) ||
        !sixel_icc_size_add(total_sample_count,
                            output_count,
                            &total_sample_count) ||
        !sixel_icc_size_add(48u, total_sample_count, &required_length) ||
        required_length > tag_length) {
        return 0;
    }

    if (input_count > SIZE_MAX / sizeof(*parsed.input_tables) ||
        clut_count > SIZE_MAX / sizeof(*parsed.clut_values) ||
        output_count > SIZE_MAX / sizeof(*parsed.output_tables)) {
        return 0;
    }

    parsed.input_tables = (uint16_t *)malloc(input_count * sizeof(*parsed.input_tables));
    parsed.clut_values = (uint16_t *)malloc(clut_count * sizeof(*parsed.clut_values));
    parsed.output_tables = (uint16_t *)malloc(output_count * sizeof(*parsed.output_tables));
    if (parsed.input_tables == NULL ||
        parsed.clut_values == NULL ||
        parsed.output_tables == NULL) {
        goto fail;
    }

    offset = 48u;
    for (i = 0u; i < input_count; ++i) {
        parsed.input_tables[i] = (uint16_t)((unsigned int)tag_data[offset + i] * 257u);
    }
    offset += input_count;

    for (i = 0u; i < clut_count; ++i) {
        parsed.clut_values[i] = (uint16_t)((unsigned int)tag_data[offset + i] * 257u);
    }
    offset += clut_count;

    for (i = 0u; i < output_count; ++i) {
        parsed.output_tables[i] = (uint16_t)((unsigned int)tag_data[offset + i] * 257u);
    }

    parsed.kind = SIXEL_ICC_LUT_MFT1;
    parsed.input_channels = input_channels;
    parsed.output_channels = output_channels;
    parsed.clut_grid_points = clut_grid_points;
    parsed.input_entries = input_entries;
    parsed.output_entries = output_entries;

    sixel_icc_lut_destroy(out_lut);
    *out_lut = parsed;
    return 1;

fail:
    sixel_icc_lut_destroy(&parsed);
    return 0;
}

static int
sixel_icc_parse_lut16_tag(unsigned char const *tag_data,
                          size_t tag_length,
                          sixel_icc_lut_t *out_lut)
{
    sixel_icc_lut_t parsed;
    unsigned char input_channels;
    unsigned char output_channels;
    unsigned char clut_grid_points;
    uint16_t input_entries;
    uint16_t output_entries;
    size_t input_count;
    size_t clut_points;
    size_t clut_count;
    size_t output_count;
    size_t total_sample_count;
    size_t total_sample_bytes;
    size_t required_length;
    size_t offset;
    size_t i;

    sixel_icc_lut_reset(&parsed);
    input_channels = 0u;
    output_channels = 0u;
    clut_grid_points = 0u;
    input_entries = 0u;
    output_entries = 0u;
    input_count = 0u;
    clut_points = 0u;
    clut_count = 0u;
    output_count = 0u;
    total_sample_count = 0u;
    total_sample_bytes = 0u;
    required_length = 0u;
    offset = 0u;
    i = 0u;

    if (tag_data == NULL || out_lut == NULL || tag_length < 52u) {
        return 0;
    }
    if (memcmp(tag_data + 0u, "mft2", 4u) != 0) {
        return 0;
    }

    input_channels = tag_data[8u];
    output_channels = tag_data[9u];
    clut_grid_points = tag_data[10u];
    input_entries = sixel_icc_read_be16(tag_data + 48u);
    output_entries = sixel_icc_read_be16(tag_data + 50u);

    if (input_channels == 0u ||
        output_channels == 0u ||
        clut_grid_points == 0u ||
        input_entries == 0u ||
        output_entries == 0u) {
        return 0;
    }

    if (!sixel_icc_size_multiply((size_t)input_channels,
                                 (size_t)input_entries,
                                 &input_count) ||
        !sixel_icc_size_pow((unsigned int)clut_grid_points,
                            (unsigned int)input_channels,
                            &clut_points) ||
        !sixel_icc_size_multiply(clut_points,
                                 (size_t)output_channels,
                                 &clut_count) ||
        !sixel_icc_size_multiply((size_t)output_channels,
                                 (size_t)output_entries,
                                 &output_count) ||
        !sixel_icc_size_add(input_count, clut_count, &total_sample_count) ||
        !sixel_icc_size_add(total_sample_count,
                            output_count,
                            &total_sample_count) ||
        !sixel_icc_size_multiply(total_sample_count, 2u, &total_sample_bytes) ||
        !sixel_icc_size_add(52u, total_sample_bytes, &required_length) ||
        required_length > tag_length) {
        return 0;
    }

    if (input_count > SIZE_MAX / sizeof(*parsed.input_tables) ||
        clut_count > SIZE_MAX / sizeof(*parsed.clut_values) ||
        output_count > SIZE_MAX / sizeof(*parsed.output_tables)) {
        return 0;
    }

    parsed.input_tables = (uint16_t *)malloc(input_count * sizeof(*parsed.input_tables));
    parsed.clut_values = (uint16_t *)malloc(clut_count * sizeof(*parsed.clut_values));
    parsed.output_tables = (uint16_t *)malloc(output_count * sizeof(*parsed.output_tables));
    if (parsed.input_tables == NULL ||
        parsed.clut_values == NULL ||
        parsed.output_tables == NULL) {
        goto fail;
    }

    offset = 52u;
    for (i = 0u; i < input_count; ++i) {
        parsed.input_tables[i] = sixel_icc_read_be16(tag_data + offset + i * 2u);
    }
    offset += input_count * 2u;

    for (i = 0u; i < clut_count; ++i) {
        parsed.clut_values[i] = sixel_icc_read_be16(tag_data + offset + i * 2u);
    }
    offset += clut_count * 2u;

    for (i = 0u; i < output_count; ++i) {
        parsed.output_tables[i] = sixel_icc_read_be16(tag_data + offset + i * 2u);
    }

    parsed.kind = SIXEL_ICC_LUT_MFT2;
    parsed.input_channels = input_channels;
    parsed.output_channels = output_channels;
    parsed.clut_grid_points = clut_grid_points;
    parsed.input_entries = input_entries;
    parsed.output_entries = output_entries;

    sixel_icc_lut_destroy(out_lut);
    *out_lut = parsed;
    return 1;

fail:
    sixel_icc_lut_destroy(&parsed);
    return 0;
}

static int
sixel_icc_parse_lut_tag(unsigned char const *profile_data,
                        size_t profile_size,
                        size_t tag_offset,
                        size_t tag_length,
                        sixel_icc_lut_t *out_lut)
{
    unsigned char const *tag_data;

    if (profile_data == NULL || out_lut == NULL) {
        return 0;
    }
    if (tag_offset > profile_size || tag_length > profile_size - tag_offset) {
        return 0;
    }
    if (tag_length < 4u) {
        return 0;
    }

    tag_data = profile_data + tag_offset;
    if (memcmp(tag_data + 0u, "mft1", 4u) == 0) {
        return sixel_icc_parse_lut8_tag(tag_data, tag_length, out_lut);
    }
    if (memcmp(tag_data + 0u, "mft2", 4u) == 0) {
        return sixel_icc_parse_lut16_tag(tag_data, tag_length, out_lut);
    }

    return 0;
}

int
sixel_icc_parse_profile(void const *data,
                        size_t length,
                        sixel_icc_profile_t *out_profile)
{
    unsigned char const *profile_data;
    size_t profile_size;
    unsigned char const *color_space;
    unsigned char const *pcs;
    sixel_icc_profile_t parsed;

    size_t rxyz_offset;
    size_t rxyz_length;
    size_t gxyz_offset;
    size_t gxyz_length;
    size_t bxyz_offset;
    size_t bxyz_length;
    size_t rtrc_offset;
    size_t rtrc_length;
    size_t gtrc_offset;
    size_t gtrc_length;
    size_t btrc_offset;
    size_t btrc_length;
    size_t wtpt_offset;
    size_t wtpt_length;
    size_t ktrc_offset;
    size_t ktrc_length;
    size_t a2b0_offset;
    size_t a2b0_length;
    double rxyz[3];
    double gxyz[3];
    double bxyz[3];
    int has_a2b0;
    int has_matrix_tags;

    if (data == NULL || length < 132u || out_profile == NULL) {
        return 0;
    }

    sixel_icc_profile_reset(&parsed);
    memset(rxyz, 0, sizeof(rxyz));
    memset(gxyz, 0, sizeof(gxyz));
    memset(bxyz, 0, sizeof(bxyz));
    has_a2b0 = 0;
    has_matrix_tags = 0;
    rxyz_offset = 0u;
    rxyz_length = 0u;
    gxyz_offset = 0u;
    gxyz_length = 0u;
    bxyz_offset = 0u;
    bxyz_length = 0u;
    rtrc_offset = 0u;
    rtrc_length = 0u;
    gtrc_offset = 0u;
    gtrc_length = 0u;
    btrc_offset = 0u;
    btrc_length = 0u;
    wtpt_offset = 0u;
    wtpt_length = 0u;
    ktrc_offset = 0u;
    ktrc_length = 0u;
    a2b0_offset = 0u;
    a2b0_length = 0u;

    profile_data = (unsigned char const *)data;
    profile_size = (size_t)sixel_icc_read_be32(profile_data + 0u);
    if (profile_size < 132u || profile_size > length) {
        return 0;
    }

    color_space = profile_data + 16u;
    pcs = profile_data + 20u;
    if (memcmp(pcs, "XYZ ", 4u) == 0) {
        parsed.pcs = SIXEL_ICC_PROFILE_PCS_XYZ;
    } else if (memcmp(pcs, "Lab ", 4u) == 0) {
        parsed.pcs = SIXEL_ICC_PROFILE_PCS_LAB;
    } else {
        return 0;
    }

    if (memcmp(color_space, "RGB ", 4u) == 0) {
        has_a2b0 = 0;
        has_matrix_tags = 0;

        if (sixel_icc_find_tag(profile_data,
                               profile_size,
                               "A2B0",
                               &a2b0_offset,
                               &a2b0_length)) {
            if (sixel_icc_parse_lut_tag(profile_data,
                                        profile_size,
                                        a2b0_offset,
                                        a2b0_length,
                                        &parsed.a2b0_lut) &&
                parsed.a2b0_lut.input_channels == 3u &&
                parsed.a2b0_lut.output_channels == 3u) {
                if (parsed.a2b0_lut.kind == SIXEL_ICC_LUT_MFT1) {
                    parsed.a2b0_lut.kind = SIXEL_ICC_LUT_MFT1_RGB_GRAY_A2B0;
                } else if (parsed.a2b0_lut.kind == SIXEL_ICC_LUT_MFT2) {
                    parsed.a2b0_lut.kind = SIXEL_ICC_LUT_MFT2_RGB_GRAY_A2B0;
                }
                has_a2b0 = 1;
            } else {
                sixel_icc_lut_destroy(&parsed.a2b0_lut);
            }
        }

        if (!has_a2b0) {
            has_matrix_tags = sixel_icc_find_tag(profile_data,
                                                 profile_size,
                                                 "rXYZ",
                                                 &rxyz_offset,
                                                 &rxyz_length)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "gXYZ",
                                      &gxyz_offset,
                                      &gxyz_length)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "bXYZ",
                                      &bxyz_offset,
                                      &bxyz_length)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "rTRC",
                                      &rtrc_offset,
                                      &rtrc_length)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "gTRC",
                                      &gtrc_offset,
                                      &gtrc_length)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "bTRC",
                                      &btrc_offset,
                                      &btrc_length);
            if (!has_matrix_tags || parsed.pcs != SIXEL_ICC_PROFILE_PCS_XYZ) {
                goto fail;
            }

            if (!sixel_icc_parse_xyz_tag(profile_data,
                                         profile_size,
                                         rxyz_offset,
                                         rxyz_length,
                                         rxyz) ||
                !sixel_icc_parse_xyz_tag(profile_data,
                                         profile_size,
                                         gxyz_offset,
                                         gxyz_length,
                                         gxyz) ||
                !sixel_icc_parse_xyz_tag(profile_data,
                                         profile_size,
                                         bxyz_offset,
                                         bxyz_length,
                                         bxyz)) {
                goto fail;
            }

            if (!sixel_icc_parse_curve_tag(profile_data,
                                           profile_size,
                                           rtrc_offset,
                                           rtrc_length,
                                           &parsed.curves[0]) ||
                !sixel_icc_parse_curve_tag(profile_data,
                                           profile_size,
                                           gtrc_offset,
                                           gtrc_length,
                                           &parsed.curves[1]) ||
                !sixel_icc_parse_curve_tag(profile_data,
                                           profile_size,
                                           btrc_offset,
                                           btrc_length,
                                           &parsed.curves[2])) {
                goto fail;
            }

            parsed.matrix_to_xyz_d50[0][0] = rxyz[0];
            parsed.matrix_to_xyz_d50[1][0] = rxyz[1];
            parsed.matrix_to_xyz_d50[2][0] = rxyz[2];

            parsed.matrix_to_xyz_d50[0][1] = gxyz[0];
            parsed.matrix_to_xyz_d50[1][1] = gxyz[1];
            parsed.matrix_to_xyz_d50[2][1] = gxyz[2];

            parsed.matrix_to_xyz_d50[0][2] = bxyz[0];
            parsed.matrix_to_xyz_d50[1][2] = bxyz[1];
            parsed.matrix_to_xyz_d50[2][2] = bxyz[2];
        }

        parsed.kind = SIXEL_ICC_PROFILE_KIND_RGB;
    } else if (memcmp(color_space, "GRAY", 4u) == 0) {
        has_a2b0 = 0;
        has_matrix_tags = 0;

        if (sixel_icc_find_tag(profile_data,
                               profile_size,
                               "A2B0",
                               &a2b0_offset,
                               &a2b0_length)) {
            if (sixel_icc_parse_lut_tag(profile_data,
                                        profile_size,
                                        a2b0_offset,
                                        a2b0_length,
                                        &parsed.a2b0_lut) &&
                parsed.a2b0_lut.input_channels == 1u &&
                parsed.a2b0_lut.output_channels == 3u) {
                if (parsed.a2b0_lut.kind == SIXEL_ICC_LUT_MFT1) {
                    parsed.a2b0_lut.kind = SIXEL_ICC_LUT_MFT1_RGB_GRAY_A2B0;
                } else if (parsed.a2b0_lut.kind == SIXEL_ICC_LUT_MFT2) {
                    parsed.a2b0_lut.kind = SIXEL_ICC_LUT_MFT2_RGB_GRAY_A2B0;
                }
                has_a2b0 = 1;
            } else {
                sixel_icc_lut_destroy(&parsed.a2b0_lut);
            }
        }

        if (!has_a2b0) {
            has_matrix_tags = sixel_icc_find_tag(profile_data,
                                                 profile_size,
                                                 "wtpt",
                                                 &wtpt_offset,
                                                 &wtpt_length)
                && sixel_icc_find_tag(profile_data,
                                      profile_size,
                                      "kTRC",
                                      &ktrc_offset,
                                      &ktrc_length);
            if (!has_matrix_tags || parsed.pcs != SIXEL_ICC_PROFILE_PCS_XYZ) {
                goto fail;
            }

            if (!sixel_icc_parse_xyz_tag(profile_data,
                                         profile_size,
                                         wtpt_offset,
                                         wtpt_length,
                                         parsed.gray_white_xyz_d50) ||
                !sixel_icc_parse_curve_tag(profile_data,
                                           profile_size,
                                           ktrc_offset,
                                           ktrc_length,
                                           &parsed.curves[0])) {
                goto fail;
            }
        }

        parsed.kind = SIXEL_ICC_PROFILE_KIND_GRAY;
    } else if (memcmp(color_space, "CMYK", 4u) == 0) {
        if (!sixel_icc_find_tag(profile_data,
                                profile_size,
                                "A2B0",
                                &a2b0_offset,
                                &a2b0_length)) {
            goto fail;
        }

        if (!sixel_icc_parse_lut_tag(profile_data,
                                     profile_size,
                                     a2b0_offset,
                                     a2b0_length,
                                     &parsed.a2b0_lut)) {
            goto fail;
        }
        if (parsed.a2b0_lut.input_channels != 4u ||
            parsed.a2b0_lut.output_channels != 3u) {
            goto fail;
        }

        parsed.kind = SIXEL_ICC_PROFILE_KIND_CMYK;
    } else {
        goto fail;
    }

    *out_profile = parsed;
    return 1;

fail:
    sixel_icc_profile_destroy(&parsed);
    return 0;
}

int
sixel_icc_parse_png_iccp(unsigned char const *png_data,
                         size_t png_size,
                         sixel_icc_profile_t *out_profile)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;

    if (png_data == NULL || out_profile == NULL) {
        return 0;
    }
    if (png_size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(png_data, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= png_size) {
        uint32_t chunk_length_u32;
        size_t chunk_length;
        size_t chunk_total;
        unsigned char const *chunk_type;
        unsigned char const *chunk_data;

        chunk_length_u32 = sixel_icc_read_be32(png_data + offset);
        chunk_length = (size_t)chunk_length_u32;
        chunk_total = chunk_length + 12u;
        if (chunk_total > png_size - offset) {
            return 0;
        }

        chunk_type = png_data + offset + 4u;
        chunk_data = png_data + offset + 8u;

        if (memcmp(chunk_type, "iCCP", 4u) == 0) {
            size_t name_len;
            size_t compressed_offset;
            unsigned char compression_method;
            unsigned char *decoded_profile;
            int decoded_length;
            int parsed;

            if (chunk_length < 3u) {
                return 0;
            }

            name_len = 0u;
            while (name_len < chunk_length && chunk_data[name_len] != 0u) {
                ++name_len;
            }
            if (name_len == chunk_length) {
                return 0;
            }

            compressed_offset = name_len + 1u;
            if (compressed_offset >= chunk_length) {
                return 0;
            }

            compression_method = chunk_data[compressed_offset];
            if (compression_method != 0u) {
                return 0;
            }

            ++compressed_offset;
            if (compressed_offset >= chunk_length) {
                return 0;
            }

            decoded_length = 0;
            decoded_profile = (unsigned char *)stbi_zlib_decode_malloc_guesssize_headerflag(
                (char const *)(chunk_data + compressed_offset),
                (int)(chunk_length - compressed_offset),
                16384,
                &decoded_length,
                1);
            if (decoded_profile == NULL || decoded_length <= 0) {
                stbi_image_free(decoded_profile);
                return 0;
            }

            parsed = sixel_icc_parse_profile(decoded_profile,
                                             (size_t)decoded_length,
                                             out_profile);
            stbi_image_free(decoded_profile);
            return parsed;
        }

        if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }

        offset += chunk_total;
    }

    return 0;
}
