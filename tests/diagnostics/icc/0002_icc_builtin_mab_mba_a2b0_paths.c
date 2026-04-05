/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Verify builtin ICC parser/apply paths for mAB/mBA A2B0 coverage.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include "src/icc-apply.h"
#include "src/icc-parse.h"

#define ICC_TAG_TABLE_OFFSET 128u
#define ICC_TAG_ENTRY_SIZE 12u

#define FIXED_0_0 0x00000000u
#define FIXED_1_0 0x00010000u

typedef struct icc_tag_ref {
    char signature[4];
    unsigned char const *data;
    size_t length;
} icc_tag_ref_t;

static void
icc_write_be16(unsigned char *p, uint16_t value)
{
    p[0] = (unsigned char)((value >> 8) & 0xffu);
    p[1] = (unsigned char)(value & 0xffu);
}

static void
icc_write_be32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)((value >> 24) & 0xffu);
    p[1] = (unsigned char)((value >> 16) & 0xffu);
    p[2] = (unsigned char)((value >> 8) & 0xffu);
    p[3] = (unsigned char)(value & 0xffu);
}

static size_t
icc_align4(size_t value)
{
    return (value + 3u) & ~(size_t)3u;
}

static size_t
icc_build_xyz_tag(unsigned char *tag, uint32_t x, uint32_t y, uint32_t z)
{
    if (tag == NULL) {
        return 0u;
    }

    memcpy(tag + 0u, "XYZ ", 4u);
    memset(tag + 4u, 0, 4u);
    icc_write_be32(tag + 8u, x);
    icc_write_be32(tag + 12u, y);
    icc_write_be32(tag + 16u, z);
    return 20u;
}

static size_t
icc_build_curv_identity_tag(unsigned char *tag)
{
    if (tag == NULL) {
        return 0u;
    }

    memcpy(tag + 0u, "curv", 4u);
    memset(tag + 4u, 0, 4u);
    icc_write_be32(tag + 8u, 0u);
    return 12u;
}

static int
icc_pack_profile(unsigned char *profile,
                 size_t profile_capacity,
                 char const color_space[4],
                 char const pcs[4],
                 icc_tag_ref_t const *tags,
                 size_t tag_count,
                 size_t *out_length)
{
    size_t table_size;
    size_t offset;
    size_t i;

    table_size = 0u;
    offset = 0u;
    i = 0u;
    if (profile == NULL || tags == NULL || out_length == NULL) {
        return 0;
    }
    if (tag_count > (SIZE_MAX - 4u) / ICC_TAG_ENTRY_SIZE) {
        return 0;
    }

    table_size = 4u + tag_count * ICC_TAG_ENTRY_SIZE;
    if (ICC_TAG_TABLE_OFFSET > SIZE_MAX - table_size) {
        return 0;
    }
    offset = ICC_TAG_TABLE_OFFSET + table_size;
    if (offset > profile_capacity) {
        return 0;
    }

    memset(profile, 0, profile_capacity);
    memcpy(profile + 16u, color_space, 4u);
    memcpy(profile + 20u, pcs, 4u);
    icc_write_be32(profile + ICC_TAG_TABLE_OFFSET, (uint32_t)tag_count);

    for (i = 0u; i < tag_count; ++i) {
        size_t entry_offset;

        if (tags[i].length > profile_capacity - offset) {
            return 0;
        }
        if (offset > UINT32_MAX || tags[i].length > UINT32_MAX) {
            return 0;
        }

        entry_offset = ICC_TAG_TABLE_OFFSET + 4u + i * ICC_TAG_ENTRY_SIZE;
        memcpy(profile + entry_offset, tags[i].signature, 4u);
        icc_write_be32(profile + entry_offset + 4u, (uint32_t)offset);
        icc_write_be32(profile + entry_offset + 8u, (uint32_t)tags[i].length);
        memcpy(profile + offset, tags[i].data, tags[i].length);
        offset += tags[i].length;
    }

    if (offset > UINT32_MAX) {
        return 0;
    }

    icc_write_be32(profile + 0u, (uint32_t)offset);
    *out_length = offset;
    return 1;
}

static void
icc_build_matrix_block(unsigned char *tag, size_t offset)
{
    size_t i;
    size_t j;

    i = 0u;
    j = 0u;
    if (tag == NULL) {
        return;
    }

    for (i = 0u; i < 3u; ++i) {
        for (j = 0u; j < 3u; ++j) {
            uint32_t value;

            value = (i == j) ? FIXED_1_0 : FIXED_0_0;
            icc_write_be32(tag + offset + (i * 3u + j) * 4u, value);
        }
    }
    for (i = 0u; i < 3u; ++i) {
        icc_write_be32(tag + offset + 36u + i * 4u, FIXED_0_0);
    }
}

static void
icc_build_curve_block(unsigned char *tag, size_t offset, size_t curve_count)
{
    size_t i;

    i = 0u;
    if (tag == NULL) {
        return;
    }

    for (i = 0u; i < curve_count; ++i) {
        (void)icc_build_curv_identity_tag(tag + offset + i * 12u);
    }
}

static void
icc_build_clut_block(unsigned char *tag,
                     size_t offset,
                     uint8_t input_channels,
                     uint8_t output_channels)
{
    size_t point_count;
    size_t point;
    size_t channel;
    size_t value_offset;

    point_count = 0u;
    point = 0u;
    channel = 0u;
    value_offset = 0u;
    if (tag == NULL || input_channels == 0u || output_channels == 0u) {
        return;
    }

    memset(tag + offset, 0, 20u);
    for (channel = 0u; channel < (size_t)input_channels; ++channel) {
        tag[offset + channel] = 2u;
    }
    tag[offset + 16u] = 2u;

    point_count = (size_t)1u << input_channels;
    value_offset = offset + 20u;
    for (point = 0u; point < point_count; ++point) {
        for (channel = 0u; channel < (size_t)output_channels; ++channel) {
            int bit;
            uint16_t value;

            bit = 0;
            value = 0u;
            if (input_channels == 1u) {
                bit = (int)(point & 1u);
            } else if (channel < (size_t)input_channels) {
                size_t shift;

                shift = (size_t)input_channels - 1u - channel;
                bit = (int)((point >> shift) & 1u);
            }
            if (bit != 0) {
                value = 65535u;
            }

            icc_write_be16(tag + value_offset, value);
            value_offset += 2u;
        }
    }
}

static size_t
icc_build_mab_identity_tag(unsigned char *tag,
                           size_t tag_capacity,
                           char const signature[4],
                           uint8_t input_channels,
                           uint8_t output_channels)
{
    size_t b_count;
    size_t m_count;
    size_t a_count;
    size_t matrix_channel_count;
    int include_matrix;
    size_t matrix_size;
    size_t point_count;
    size_t clut_values;
    size_t b_offset;
    size_t matrix_offset;
    size_t m_offset;
    size_t clut_offset;
    size_t a_offset;
    size_t total_length;

    b_count = 0u;
    m_count = 0u;
    a_count = 0u;
    matrix_channel_count = 0u;
    include_matrix = 0;
    matrix_size = 0u;
    point_count = 0u;
    clut_values = 0u;
    b_offset = 0u;
    matrix_offset = 0u;
    m_offset = 0u;
    clut_offset = 0u;
    a_offset = 0u;
    total_length = 0u;
    if (tag == NULL || signature == NULL || input_channels == 0u ||
        output_channels == 0u || input_channels > 16u ||
        output_channels > 16u || input_channels >= sizeof(size_t) * 8u) {
        return 0u;
    }

    if (memcmp(signature, "mAB ", 4u) == 0) {
        b_count = (size_t)output_channels;
        m_count = (size_t)output_channels;
        a_count = (size_t)input_channels;
        matrix_channel_count = (size_t)output_channels;
    } else if (memcmp(signature, "mBA ", 4u) == 0) {
        b_count = (size_t)input_channels;
        m_count = (size_t)input_channels;
        a_count = (size_t)output_channels;
        matrix_channel_count = (size_t)input_channels;
    } else {
        return 0u;
    }
    include_matrix = matrix_channel_count == 3u;
    matrix_size = include_matrix ? 48u : 0u;

    point_count = (size_t)1u << input_channels;
    clut_values = point_count * (size_t)output_channels;

    b_offset = 32u;
    matrix_offset = include_matrix
        ? icc_align4(b_offset + b_count * 12u)
        : 0u;
    m_offset = include_matrix
        ? icc_align4(matrix_offset + matrix_size)
        : icc_align4(b_offset + b_count * 12u);
    clut_offset = icc_align4(m_offset + m_count * 12u);
    a_offset = icc_align4(clut_offset + 20u + clut_values * 2u);
    total_length = a_offset + a_count * 12u;

    if (total_length > tag_capacity) {
        return 0u;
    }

    memset(tag, 0, total_length);
    memcpy(tag + 0u, signature, 4u);
    tag[8u] = input_channels;
    tag[9u] = output_channels;

    icc_write_be32(tag + 12u, (uint32_t)b_offset);
    icc_write_be32(tag + 16u, (uint32_t)matrix_offset);
    icc_write_be32(tag + 20u, (uint32_t)m_offset);
    icc_write_be32(tag + 24u, (uint32_t)clut_offset);
    icc_write_be32(tag + 28u, (uint32_t)a_offset);

    icc_build_curve_block(tag, b_offset, b_count);
    if (include_matrix) {
        icc_build_matrix_block(tag, matrix_offset);
    }
    icc_build_curve_block(tag, m_offset, m_count);
    icc_build_clut_block(tag, clut_offset, input_channels, output_channels);
    icc_build_curve_block(tag, a_offset, a_count);

    return total_length;
}

static int
icc_build_rgb_mab_profile(unsigned char *profile,
                          size_t profile_capacity,
                          char const signature[4],
                          int with_matrix_trc,
                          uint8_t a2b0_output_channels,
                          int truncate_a2b0,
                          size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    unsigned char xyz_tag[20];
    unsigned char trc_tag[12];
    icc_tag_ref_t tags[7];
    size_t a2b0_length;
    size_t tag_count;

    a2b0_length = 0u;
    tag_count = 0u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || signature == NULL || out_length == NULL) {
        return 0;
    }

    a2b0_length = icc_build_mab_identity_tag(a2b0_tag,
                                             sizeof(a2b0_tag),
                                             signature,
                                             3u,
                                             a2b0_output_channels);
    if (a2b0_length == 0u) {
        return 0;
    }
    if (truncate_a2b0 && a2b0_length > 4u) {
        a2b0_length -= 4u;
    }

    if (with_matrix_trc) {
        icc_build_xyz_tag(xyz_tag, FIXED_0_0, FIXED_0_0, FIXED_0_0);
        icc_build_curv_identity_tag(trc_tag);

        memcpy(tags[tag_count].signature, "rXYZ", 4u);
        tags[tag_count].data = xyz_tag;
        tags[tag_count].length = sizeof(xyz_tag);
        ++tag_count;

        memcpy(tags[tag_count].signature, "gXYZ", 4u);
        tags[tag_count].data = xyz_tag;
        tags[tag_count].length = sizeof(xyz_tag);
        ++tag_count;

        memcpy(tags[tag_count].signature, "bXYZ", 4u);
        tags[tag_count].data = xyz_tag;
        tags[tag_count].length = sizeof(xyz_tag);
        ++tag_count;

        memcpy(tags[tag_count].signature, "rTRC", 4u);
        tags[tag_count].data = trc_tag;
        tags[tag_count].length = sizeof(trc_tag);
        ++tag_count;

        memcpy(tags[tag_count].signature, "gTRC", 4u);
        tags[tag_count].data = trc_tag;
        tags[tag_count].length = sizeof(trc_tag);
        ++tag_count;

        memcpy(tags[tag_count].signature, "bTRC", 4u);
        tags[tag_count].data = trc_tag;
        tags[tag_count].length = sizeof(trc_tag);
        ++tag_count;
    }

    memcpy(tags[tag_count].signature, "A2B0", 4u);
    tags[tag_count].data = a2b0_tag;
    tags[tag_count].length = a2b0_length;
    ++tag_count;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "RGB ",
                            "XYZ ",
                            tags,
                            tag_count,
                            out_length);
}

static int
icc_build_gray_mab_profile(unsigned char *profile,
                           size_t profile_capacity,
                           char const signature[4],
                           uint8_t a2b0_output_channels,
                           size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    icc_tag_ref_t tags[1];
    size_t a2b0_length;

    a2b0_length = 0u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || signature == NULL || out_length == NULL) {
        return 0;
    }

    a2b0_length = icc_build_mab_identity_tag(a2b0_tag,
                                             sizeof(a2b0_tag),
                                             signature,
                                             1u,
                                             a2b0_output_channels);
    if (a2b0_length == 0u) {
        return 0;
    }

    memcpy(tags[0].signature, "A2B0", 4u);
    tags[0].data = a2b0_tag;
    tags[0].length = a2b0_length;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "GRAY",
                            "XYZ ",
                            tags,
                            1u,
                            out_length);
}

static int
icc_build_cmyk_mab_profile(unsigned char *profile,
                           size_t profile_capacity,
                           char const signature[4],
                           uint8_t a2b0_output_channels,
                           size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    icc_tag_ref_t tags[1];
    size_t a2b0_length;

    a2b0_length = 0u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || signature == NULL || out_length == NULL) {
        return 0;
    }

    a2b0_length = icc_build_mab_identity_tag(a2b0_tag,
                                             sizeof(a2b0_tag),
                                             signature,
                                             4u,
                                             a2b0_output_channels);
    if (a2b0_length == 0u) {
        return 0;
    }

    memcpy(tags[0].signature, "A2B0", 4u);
    tags[0].data = a2b0_tag;
    tags[0].length = a2b0_length;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "CMYK",
                            "XYZ ",
                            tags,
                            1u,
                            out_length);
}

static int
run_rgb_mab_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double rgb[3];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));

    if (!icc_build_rgb_mab_profile(profile,
                                   sizeof(profile),
                                   "mAB ",
                                   1,
                                   3u,
                                   0,
                                   &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_RGB ||
        parsed.a2b_mab[0].type != SIXEL_ICC_MAB_TYPE_MAB ||
        parsed.a2b_mab[0].input_channels != 3u ||
        parsed.a2b_mab[0].output_channels != 3u) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    /* mAB path should win over zero matrix/TRC fallback. */
    rgb[0] = 1.0;
    rgb[1] = 0.0;
    rgb[2] = 0.0;
    if (!sixel_icc_apply_rgb_triplet_unit(rgb, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    if (rgb[0] <= 0.4 || rgb[1] >= 0.4 || rgb[2] >= 0.4) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_rgb_mab_profile(profile,
                                   sizeof(profile),
                                   "mBA ",
                                   0,
                                   3u,
                                   0,
                                   &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed) ||
        parsed.a2b_mab[0].type != SIXEL_ICC_MAB_TYPE_MBA) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_rgb_mab_profile(profile,
                                   sizeof(profile),
                                   "mAB ",
                                   0,
                                   2u,
                                   0,
                                   &profile_length)) {
        return 0;
    }
    if (sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    if (!icc_build_rgb_mab_profile(profile,
                                   sizeof(profile),
                                   "mAB ",
                                   1,
                                   2u,
                                   0,
                                   &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.a2b_mab[0].type != SIXEL_ICC_MAB_TYPE_INVALID ||
        parsed.curves[0].kind == SIXEL_ICC_CURVE_INVALID) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_rgb_mab_profile(profile,
                                   sizeof(profile),
                                   "mAB ",
                                   1,
                                   3u,
                                   1,
                                   &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.a2b_mab[0].type != SIXEL_ICC_MAB_TYPE_INVALID ||
        parsed.curves[0].kind == SIXEL_ICC_CURVE_INVALID) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    return 1;
}

static int
run_gray_mab_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double rgb[3];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));

    if (!icc_build_gray_mab_profile(profile,
                                    sizeof(profile),
                                    "mAB ",
                                    3u,
                                    &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_GRAY ||
        parsed.a2b_mab[0].type != SIXEL_ICC_MAB_TYPE_MAB ||
        parsed.a2b_mab[0].input_channels != 1u ||
        parsed.a2b_mab[0].output_channels != 3u) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    rgb[0] = 0.75;
    rgb[1] = 0.75;
    rgb[2] = 0.75;
    if (!sixel_icc_apply_rgb_triplet_unit(rgb, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    if (rgb[0] < 0.2 || rgb[1] < 0.2 || rgb[2] < 0.2) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_gray_mab_profile(profile,
                                    sizeof(profile),
                                    "mBA ",
                                    3u,
                                    &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed) ||
        parsed.a2b_mab[0].type != SIXEL_ICC_MAB_TYPE_MBA) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_gray_mab_profile(profile,
                                    sizeof(profile),
                                    "mAB ",
                                    2u,
                                    &profile_length)) {
        return 0;
    }
    if (sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    return 1;
}

static int
run_cmyk_mab_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    unsigned char cmyk_u8[4];
    float rgb_float[3];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));
    memset(cmyk_u8, 0, sizeof(cmyk_u8));
    memset(rgb_float, 0, sizeof(rgb_float));

    if (!icc_build_cmyk_mab_profile(profile,
                                    sizeof(profile),
                                    "mAB ",
                                    3u,
                                    &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_CMYK ||
        parsed.a2b_mab[0].type != SIXEL_ICC_MAB_TYPE_MAB ||
        parsed.a2b_mab[0].input_channels != 4u ||
        parsed.a2b_mab[0].output_channels != 3u) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    cmyk_u8[0] = 255u;
    cmyk_u8[1] = 0u;
    cmyk_u8[2] = 0u;
    cmyk_u8[3] = 0u;
    if (!sixel_icc_apply_cmyk_u8_to_rgb_float32(rgb_float,
                                                 cmyk_u8,
                                                 1u,
                                                 &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    if (rgb_float[0] <= 0.4f || rgb_float[1] >= 0.4f ||
        rgb_float[2] >= 0.4f) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_cmyk_mab_profile(profile,
                                    sizeof(profile),
                                    "mBA ",
                                    3u,
                                    &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed) ||
        parsed.a2b_mab[0].type != SIXEL_ICC_MAB_TYPE_MBA) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_cmyk_mab_profile(profile,
                                    sizeof(profile),
                                    "mAB ",
                                    2u,
                                    &profile_length)) {
        return 0;
    }
    if (sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    return 1;
}

int
test_icc_0002_icc_builtin_mab_mba_a2b0_paths(int argc, char **argv)
{
    int success;

    (void)argc;
    (void)argv;

    success = 1;
    if (!run_rgb_mab_cases()) {
        fprintf(stderr, "RGB mAB/mBA coverage cases failed\n");
        success = 0;
    }
    if (!run_gray_mab_cases()) {
        fprintf(stderr, "GRAY mAB/mBA coverage cases failed\n");
        success = 0;
    }
    if (!run_cmyk_mab_cases()) {
        fprintf(stderr, "CMYK mAB/mBA coverage cases failed\n");
        success = 0;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
