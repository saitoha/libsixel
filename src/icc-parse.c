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
sixel_icc_profile_reset(sixel_icc_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }

    memset(profile, 0, sizeof(*profile));
    profile->kind = SIXEL_ICC_PROFILE_KIND_INVALID;
    sixel_icc_curve_reset(&profile->curves[0]);
    sixel_icc_curve_reset(&profile->curves[1]);
    sixel_icc_curve_reset(&profile->curves[2]);
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

    profile->kind = SIXEL_ICC_PROFILE_KIND_INVALID;
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
    x = sixel_icc_clamp_unit(x);

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

    return sixel_icc_clamp_unit(y);
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

            out_curve->table[i] = (uint16_t)(sixel_icc_clamp_unit(y) * 65535.0 + 0.5);
        }
        return 1;
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
    double rxyz[3];
    double gxyz[3];
    double bxyz[3];

    if (data == NULL || length < 132u || out_profile == NULL) {
        return 0;
    }

    sixel_icc_profile_reset(&parsed);
    memset(rxyz, 0, sizeof(rxyz));
    memset(gxyz, 0, sizeof(gxyz));
    memset(bxyz, 0, sizeof(bxyz));

    profile_data = (unsigned char const *)data;
    profile_size = (size_t)sixel_icc_read_be32(profile_data + 0u);
    if (profile_size < 132u || profile_size > length) {
        return 0;
    }

    color_space = profile_data + 16u;
    pcs = profile_data + 20u;
    if (memcmp(pcs, "XYZ ", 4u) != 0) {
        return 0;
    }

    if (memcmp(color_space, "RGB ", 4u) == 0) {
        if (!sixel_icc_find_tag(profile_data, profile_size,
                                "rXYZ", &rxyz_offset, &rxyz_length) ||
            !sixel_icc_find_tag(profile_data, profile_size,
                                "gXYZ", &gxyz_offset, &gxyz_length) ||
            !sixel_icc_find_tag(profile_data, profile_size,
                                "bXYZ", &bxyz_offset, &bxyz_length) ||
            !sixel_icc_find_tag(profile_data, profile_size,
                                "rTRC", &rtrc_offset, &rtrc_length) ||
            !sixel_icc_find_tag(profile_data, profile_size,
                                "gTRC", &gtrc_offset, &gtrc_length) ||
            !sixel_icc_find_tag(profile_data, profile_size,
                                "bTRC", &btrc_offset, &btrc_length)) {
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

        parsed.kind = SIXEL_ICC_PROFILE_KIND_RGB;
    } else if (memcmp(color_space, "GRAY", 4u) == 0) {
        if (!sixel_icc_find_tag(profile_data,
                                profile_size,
                                "wtpt",
                                &wtpt_offset,
                                &wtpt_length) ||
            !sixel_icc_find_tag(profile_data,
                                profile_size,
                                "kTRC",
                                &ktrc_offset,
                                &ktrc_length)) {
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

        parsed.kind = SIXEL_ICC_PROFILE_KIND_GRAY;
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
