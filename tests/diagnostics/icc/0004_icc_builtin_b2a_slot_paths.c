/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Verify builtin ICC B2A0/B2A1/B2A2 parser and apply paths.
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

/*
 * Prefix file-local ICC helper symbols so unity/amalgamation builds that
 * include multiple ICC test sources in one translation unit do not collide.
 */
#define icc_tag_ref icc0004_tag_ref
#define icc_tag_ref_t icc0004_tag_ref_t
#define icc_write_be16 icc0004_write_be16
#define icc_write_be32 icc0004_write_be32
#define icc_align4 icc0004_align4
#define icc_build_curv_identity_tag icc0004_build_curv_identity_tag
#define icc_build_curve_block icc0004_build_curve_block
#define icc_build_matrix_block icc0004_build_matrix_block
#define icc_build_clut_block_mode icc0004_build_clut_block_mode
#define icc_build_mab_mode_tag icc0004_build_mab_mode_tag
#define icc_pack_profile icc0004_pack_profile
#define icc_build_rgb_profile_with_b2a icc0004_build_rgb_profile_with_b2a
#define icc_build_gray_profile_with_b2a icc0004_build_gray_profile_with_b2a
#define icc_build_cmyk_profile_with_b2a icc0004_build_cmyk_profile_with_b2a
#define expect_slot_channel icc0004_expect_slot_channel
#define expect_cmyk_slot icc0004_expect_cmyk_slot

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
icc_build_matrix_block(unsigned char *tag, size_t offset)
{
    size_t i;
    size_t j;
    uint32_t value;

    i = 0u;
    j = 0u;
    value = FIXED_0_0;
    if (tag == NULL) {
        return;
    }

    for (i = 0u; i < 3u; ++i) {
        for (j = 0u; j < 3u; ++j) {
            value = (i == j) ? FIXED_1_0 : FIXED_0_0;
            icc_write_be32(tag + offset + (i * 3u + j) * 4u, value);
        }
    }
    for (i = 0u; i < 3u; ++i) {
        icc_write_be32(tag + offset + 36u + i * 4u, FIXED_0_0);
    }
}

static void
icc_build_clut_block_mode(unsigned char *tag,
                          size_t offset,
                          uint8_t input_channels,
                          uint8_t output_channels,
                          unsigned int mode)
{
    size_t point_count;
    size_t point;
    size_t channel;
    size_t value_offset;
    size_t shift;
    int bit0;

    point_count = 0u;
    point = 0u;
    channel = 0u;
    value_offset = 0u;
    shift = 0u;
    bit0 = 0;
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
    shift = (input_channels > 1u) ? (size_t)input_channels - 1u : 0u;
    for (point = 0u; point < point_count; ++point) {
        bit0 = (int)((point >> shift) & 1u);
        for (channel = 0u; channel < (size_t)output_channels; ++channel) {
            uint16_t value;

            value = 0u;
            if (bit0 != 0) {
                if (output_channels == 1u) {
                    value = (uint16_t)((mode + 1u) * 21845u);
                } else if ((unsigned int)channel == mode) {
                    value = 65535u;
                }
            }
            icc_write_be16(tag + value_offset, value);
            value_offset += 2u;
        }
    }
}

static size_t
icc_build_mab_mode_tag(unsigned char *tag,
                       size_t tag_capacity,
                       char const signature[4],
                       uint8_t input_channels,
                       uint8_t output_channels,
                       unsigned int mode)
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
        output_channels > 16u || input_channels >= sizeof(size_t) * 8u ||
        mode >= 4u) {
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
    matrix_offset = include_matrix ? icc_align4(b_offset + b_count * 12u) : 0u;
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
    icc_build_clut_block_mode(tag,
                              clut_offset,
                              input_channels,
                              output_channels,
                              mode);
    icc_build_curve_block(tag, a_offset, a_count);

    return total_length;
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

    table_size = 4u + tag_count * ICC_TAG_ENTRY_SIZE;
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

        entry_offset = ICC_TAG_TABLE_OFFSET + 4u + i * ICC_TAG_ENTRY_SIZE;
        memcpy(profile + entry_offset, tags[i].signature, 4u);
        icc_write_be32(profile + entry_offset + 4u, (uint32_t)offset);
        icc_write_be32(profile + entry_offset + 8u, (uint32_t)tags[i].length);
        memcpy(profile + offset, tags[i].data, tags[i].length);
        offset += tags[i].length;
    }

    icc_write_be32(profile + 0u, (uint32_t)offset);
    *out_length = offset;
    return 1;
}

static int
icc_build_rgb_profile_with_b2a(unsigned char *profile,
                               size_t profile_capacity,
                               char const pcs[4],
                               int truncate_b2a1,
                               int mismatch_b2a1,
                               size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    unsigned char b2a0_tag[4096];
    unsigned char b2a1_tag[4096];
    unsigned char b2a2_tag[4096];
    icc_tag_ref_t tags[4];
    size_t a2b0_length;
    size_t b2a0_length;
    size_t b2a1_length;
    size_t b2a2_length;
    uint8_t b2a1_outputs;

    a2b0_length = 0u;
    b2a0_length = 0u;
    b2a1_length = 0u;
    b2a2_length = 0u;
    b2a1_outputs = 3u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || pcs == NULL || out_length == NULL) {
        return 0;
    }

    if (mismatch_b2a1) {
        b2a1_outputs = 2u;
    }

    a2b0_length = icc_build_mab_mode_tag(a2b0_tag,
                                          sizeof(a2b0_tag),
                                          "mAB ",
                                          3u,
                                          3u,
                                          0u);
    b2a0_length = icc_build_mab_mode_tag(b2a0_tag,
                                          sizeof(b2a0_tag),
                                          "mAB ",
                                          3u,
                                          3u,
                                          0u);
    b2a1_length = icc_build_mab_mode_tag(b2a1_tag,
                                          sizeof(b2a1_tag),
                                          "mAB ",
                                          3u,
                                          b2a1_outputs,
                                          1u);
    b2a2_length = icc_build_mab_mode_tag(b2a2_tag,
                                          sizeof(b2a2_tag),
                                          "mAB ",
                                          3u,
                                          3u,
                                          2u);
    if (a2b0_length == 0u || b2a0_length == 0u ||
        b2a1_length == 0u || b2a2_length == 0u) {
        return 0;
    }
    if (truncate_b2a1 && b2a1_length > 4u) {
        b2a1_length -= 4u;
    }

    memcpy(tags[0].signature, "A2B0", 4u);
    tags[0].data = a2b0_tag;
    tags[0].length = a2b0_length;

    memcpy(tags[1].signature, "B2A0", 4u);
    tags[1].data = b2a0_tag;
    tags[1].length = b2a0_length;

    memcpy(tags[2].signature, "B2A1", 4u);
    tags[2].data = b2a1_tag;
    tags[2].length = b2a1_length;

    memcpy(tags[3].signature, "B2A2", 4u);
    tags[3].data = b2a2_tag;
    tags[3].length = b2a2_length;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "RGB ",
                            pcs,
                            tags,
                            4u,
                            out_length);
}

static int
icc_build_gray_profile_with_b2a(unsigned char *profile,
                                size_t profile_capacity,
                                size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    unsigned char b2a0_tag[4096];
    unsigned char b2a1_tag[4096];
    unsigned char b2a2_tag[4096];
    icc_tag_ref_t tags[4];
    size_t a2b0_length;
    size_t b2a0_length;
    size_t b2a1_length;
    size_t b2a2_length;

    a2b0_length = 0u;
    b2a0_length = 0u;
    b2a1_length = 0u;
    b2a2_length = 0u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || out_length == NULL) {
        return 0;
    }

    a2b0_length = icc_build_mab_mode_tag(a2b0_tag,
                                          sizeof(a2b0_tag),
                                          "mAB ",
                                          1u,
                                          3u,
                                          0u);
    b2a0_length = icc_build_mab_mode_tag(b2a0_tag,
                                          sizeof(b2a0_tag),
                                          "mAB ",
                                          3u,
                                          1u,
                                          0u);
    b2a1_length = icc_build_mab_mode_tag(b2a1_tag,
                                          sizeof(b2a1_tag),
                                          "mAB ",
                                          3u,
                                          1u,
                                          1u);
    b2a2_length = icc_build_mab_mode_tag(b2a2_tag,
                                          sizeof(b2a2_tag),
                                          "mAB ",
                                          3u,
                                          1u,
                                          2u);
    if (a2b0_length == 0u || b2a0_length == 0u ||
        b2a1_length == 0u || b2a2_length == 0u) {
        return 0;
    }

    memcpy(tags[0].signature, "A2B0", 4u);
    tags[0].data = a2b0_tag;
    tags[0].length = a2b0_length;

    memcpy(tags[1].signature, "B2A0", 4u);
    tags[1].data = b2a0_tag;
    tags[1].length = b2a0_length;

    memcpy(tags[2].signature, "B2A1", 4u);
    tags[2].data = b2a1_tag;
    tags[2].length = b2a1_length;

    memcpy(tags[3].signature, "B2A2", 4u);
    tags[3].data = b2a2_tag;
    tags[3].length = b2a2_length;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "GRAY",
                            "XYZ ",
                            tags,
                            4u,
                            out_length);
}

static int
icc_build_cmyk_profile_with_b2a(unsigned char *profile,
                                size_t profile_capacity,
                                size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    unsigned char b2a0_tag[4096];
    unsigned char b2a1_tag[4096];
    unsigned char b2a2_tag[4096];
    icc_tag_ref_t tags[4];
    size_t a2b0_length;
    size_t b2a0_length;
    size_t b2a1_length;
    size_t b2a2_length;

    a2b0_length = 0u;
    b2a0_length = 0u;
    b2a1_length = 0u;
    b2a2_length = 0u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || out_length == NULL) {
        return 0;
    }

    a2b0_length = icc_build_mab_mode_tag(a2b0_tag,
                                          sizeof(a2b0_tag),
                                          "mAB ",
                                          4u,
                                          3u,
                                          0u);
    b2a0_length = icc_build_mab_mode_tag(b2a0_tag,
                                          sizeof(b2a0_tag),
                                          "mAB ",
                                          3u,
                                          4u,
                                          0u);
    b2a1_length = icc_build_mab_mode_tag(b2a1_tag,
                                          sizeof(b2a1_tag),
                                          "mAB ",
                                          3u,
                                          4u,
                                          1u);
    b2a2_length = icc_build_mab_mode_tag(b2a2_tag,
                                          sizeof(b2a2_tag),
                                          "mAB ",
                                          3u,
                                          4u,
                                          2u);
    if (a2b0_length == 0u || b2a0_length == 0u ||
        b2a1_length == 0u || b2a2_length == 0u) {
        return 0;
    }

    memcpy(tags[0].signature, "A2B0", 4u);
    tags[0].data = a2b0_tag;
    tags[0].length = a2b0_length;

    memcpy(tags[1].signature, "B2A0", 4u);
    tags[1].data = b2a0_tag;
    tags[1].length = b2a0_length;

    memcpy(tags[2].signature, "B2A1", 4u);
    tags[2].data = b2a1_tag;
    tags[2].length = b2a1_length;

    memcpy(tags[3].signature, "B2A2", 4u);
    tags[3].data = b2a2_tag;
    tags[3].length = b2a2_length;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "CMYK",
                            "XYZ ",
                            tags,
                            4u,
                            out_length);
}

static int
expect_slot_channel(double const rgb[3], unsigned int slot)
{
    if (slot == 0u) {
        return rgb[0] > 0.4 && rgb[1] < 0.3 && rgb[2] < 0.3;
    }
    if (slot == 1u) {
        return rgb[0] < 0.3 && rgb[1] > 0.4 && rgb[2] < 0.3;
    }
    if (slot == 2u) {
        return rgb[0] < 0.3 && rgb[1] < 0.3 && rgb[2] > 0.4;
    }

    return 0;
}

static int
expect_cmyk_slot(double const cmyk[4], unsigned int slot)
{
    if (slot >= 4u) {
        return 0;
    }

    if (slot == 0u) {
        return cmyk[0] > 0.5 && cmyk[1] < 0.3 &&
            cmyk[2] < 0.3 && cmyk[3] < 0.3;
    }
    if (slot == 1u) {
        return cmyk[0] < 0.3 && cmyk[1] > 0.5 &&
            cmyk[2] < 0.3 && cmyk[3] < 0.3;
    }
    if (slot == 2u) {
        return cmyk[0] < 0.3 && cmyk[1] < 0.3 &&
            cmyk[2] > 0.5 && cmyk[3] < 0.3;
    }

    return cmyk[0] < 0.3 && cmyk[1] < 0.3 &&
        cmyk[2] < 0.3 && cmyk[3] > 0.5;
}

static int
run_rgb_b2a_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double xyz_d50[3];
    double rgb[3];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));
    memset(xyz_d50, 0, sizeof(xyz_d50));
    memset(rgb, 0, sizeof(rgb));

    if (!icc_build_rgb_profile_with_b2a(profile,
                                        sizeof(profile),
                                        "XYZ ",
                                        0,
                                        0,
                                        &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.b2a_mab[0].type != SIXEL_ICC_MAB_TYPE_MAB ||
        parsed.b2a_mab[1].type != SIXEL_ICC_MAB_TYPE_MAB ||
        parsed.b2a_mab[2].type != SIXEL_ICC_MAB_TYPE_MAB ||
        parsed.b2a_mab[0].input_channels != 3u ||
        parsed.b2a_mab[0].output_channels != 3u) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    xyz_d50[0] = 1.0;
    xyz_d50[1] = 0.0;
    xyz_d50[2] = 0.0;
    if (!sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(rgb,
                                                          3u,
                                                          xyz_d50,
                                                          &parsed,
                                                          0u) ||
        !expect_slot_channel(rgb, 0u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    xyz_d50[0] = 1.0;
    xyz_d50[1] = 0.0;
    xyz_d50[2] = 0.0;
    if (!sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(rgb,
                                                          3u,
                                                          xyz_d50,
                                                          &parsed,
                                                          1u) ||
        !expect_slot_channel(rgb, 1u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    xyz_d50[0] = 1.0;
    xyz_d50[1] = 0.0;
    xyz_d50[2] = 0.0;
    if (!sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(rgb,
                                                          3u,
                                                          xyz_d50,
                                                          &parsed,
                                                          2u) ||
        !expect_slot_channel(rgb, 2u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_rgb_profile_with_b2a(profile,
                                        sizeof(profile),
                                        "XYZ ",
                                        1,
                                        0,
                                        &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.b2a_mab[0].type == SIXEL_ICC_MAB_TYPE_INVALID ||
        parsed.b2a_mab[1].type != SIXEL_ICC_MAB_TYPE_INVALID ||
        parsed.b2a_mab[2].type == SIXEL_ICC_MAB_TYPE_INVALID) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    xyz_d50[0] = 1.0;
    xyz_d50[1] = 0.0;
    xyz_d50[2] = 0.0;
    if (sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(rgb,
                                                         3u,
                                                         xyz_d50,
                                                         &parsed,
                                                         1u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    if (!sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(rgb,
                                                          3u,
                                                          xyz_d50,
                                                          &parsed,
                                                          2u) ||
        !expect_slot_channel(rgb, 2u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_rgb_profile_with_b2a(profile,
                                        sizeof(profile),
                                        "XYZ ",
                                        0,
                                        1,
                                        &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.b2a_mab[1].type != SIXEL_ICC_MAB_TYPE_INVALID) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    return 1;
}

static int
run_gray_b2a_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double xyz_d50[3];
    double gray0[1];
    double gray1[1];
    double gray2[1];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));
    memset(xyz_d50, 0, sizeof(xyz_d50));
    memset(gray0, 0, sizeof(gray0));
    memset(gray1, 0, sizeof(gray1));
    memset(gray2, 0, sizeof(gray2));

    if (!icc_build_gray_profile_with_b2a(profile,
                                         sizeof(profile),
                                         &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }

    xyz_d50[0] = 1.0;
    xyz_d50[1] = 0.0;
    xyz_d50[2] = 0.0;
    if (!sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(gray0,
                                                          1u,
                                                          xyz_d50,
                                                          &parsed,
                                                          0u) ||
        !sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(gray1,
                                                          1u,
                                                          xyz_d50,
                                                          &parsed,
                                                          1u) ||
        !sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(gray2,
                                                          1u,
                                                          xyz_d50,
                                                          &parsed,
                                                          2u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    if (!(gray0[0] < gray1[0] && gray1[0] < gray2[0])) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    sixel_icc_profile_destroy(&parsed);
    return 1;
}

static int
run_cmyk_b2a_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double xyz_d50[3];
    double cmyk[4];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));
    memset(xyz_d50, 0, sizeof(xyz_d50));
    memset(cmyk, 0, sizeof(cmyk));

    if (!icc_build_cmyk_profile_with_b2a(profile,
                                         sizeof(profile),
                                         &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }

    xyz_d50[0] = 1.0;
    xyz_d50[1] = 0.0;
    xyz_d50[2] = 0.0;
    if (!sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(cmyk,
                                                          4u,
                                                          xyz_d50,
                                                          &parsed,
                                                          2u) ||
        !expect_cmyk_slot(cmyk, 2u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    sixel_icc_profile_destroy(&parsed);
    return 1;
}

static int
run_lab_pcs_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double xyz_d50[3];
    double rgb[3];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));
    memset(xyz_d50, 0, sizeof(xyz_d50));
    memset(rgb, 0, sizeof(rgb));

    if (!icc_build_rgb_profile_with_b2a(profile,
                                        sizeof(profile),
                                        "Lab ",
                                        0,
                                        0,
                                        &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }

    xyz_d50[0] = 0.9642;
    xyz_d50[1] = 1.0000;
    xyz_d50[2] = 0.8249;
    if (!sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(rgb,
                                                          3u,
                                                          xyz_d50,
                                                          &parsed,
                                                          1u) ||
        !expect_slot_channel(rgb, 1u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    sixel_icc_profile_destroy(&parsed);
    return 1;
}

int
test_icc_0004_icc_builtin_b2a_slot_paths(int argc, char **argv)
{
    int ok;

    (void)argc;
    (void)argv;

    ok = 1;
    if (!run_rgb_b2a_cases()) {
        fprintf(stderr, "RGB B2A slot parser/apply cases failed\n");
        ok = 0;
    }
    if (!run_gray_b2a_cases()) {
        fprintf(stderr, "GRAY B2A slot parser/apply cases failed\n");
        ok = 0;
    }
    if (!run_cmyk_b2a_cases()) {
        fprintf(stderr, "CMYK B2A slot parser/apply cases failed\n");
        ok = 0;
    }
    if (!run_lab_pcs_cases()) {
        fprintf(stderr, "Lab PCS B2A slot parser/apply cases failed\n");
        ok = 0;
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

#undef expect_cmyk_slot
#undef expect_slot_channel
#undef icc_build_cmyk_profile_with_b2a
#undef icc_build_gray_profile_with_b2a
#undef icc_build_rgb_profile_with_b2a
#undef icc_pack_profile
#undef icc_build_mab_mode_tag
#undef icc_build_clut_block_mode
#undef icc_build_matrix_block
#undef icc_build_curve_block
#undef icc_build_curv_identity_tag
#undef icc_align4
#undef icc_write_be32
#undef icc_write_be16
#undef icc_tag_ref_t
#undef icc_tag_ref

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
