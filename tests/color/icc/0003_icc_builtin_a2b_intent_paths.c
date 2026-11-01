/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Verify builtin ICC A2B0/A2B1/A2B2 slot parsing and intent selection.
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

#include "src/cms.h"
#include "src/icc-apply.h"
#include "src/icc-parse.h"

/*
 * Prefix file-local ICC helper symbols so unity/amalgamation builds that
 * include multiple ICC test sources in one translation unit do not collide.
 */
#define icc_tag_ref icc0003_tag_ref
#define icc_tag_ref_t icc0003_tag_ref_t
#define icc_write_be16 icc0003_write_be16
#define icc_write_be32 icc0003_write_be32
#define icc_align4 icc0003_align4
#define icc_build_curv_identity_tag icc0003_build_curv_identity_tag
#define icc_build_curve_block icc0003_build_curve_block
#define icc_build_matrix_block icc0003_build_matrix_block
#define icc_build_clut_block_mode icc0003_build_clut_block_mode
#define icc_build_mab_mode_tag icc0003_build_mab_mode_tag
#define icc_build_xyz_tag icc0003_build_xyz_tag
#define icc_pack_profile icc0003_pack_profile
#define icc_build_rgb_profile icc0003_build_rgb_profile
#define icc_build_gray_profile icc0003_build_gray_profile
#define icc_build_cmyk_profile icc0003_build_cmyk_profile
#define expect_slot_channel icc0003_expect_slot_channel
#define check_transform_rgb8 icc0003_check_transform_rgb8
#define test_setenv icc0003_test_setenv

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
            if ((unsigned int)channel == mode && bit0 != 0) {
                value = 65535u;
            }
            icc_write_be16(tag + value_offset, value);
            value_offset += 2u;
        }
    }
}

static size_t
icc_build_mab_mode_tag(unsigned char *tag,
                       size_t tag_capacity,
                       uint8_t input_channels,
                       uint8_t output_channels,
                       unsigned int mode)
{
    size_t b_count;
    size_t m_count;
    size_t a_count;
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
    point_count = 0u;
    clut_values = 0u;
    b_offset = 0u;
    matrix_offset = 0u;
    m_offset = 0u;
    clut_offset = 0u;
    a_offset = 0u;
    total_length = 0u;
    if (tag == NULL || input_channels == 0u || output_channels == 0u ||
        input_channels > 16u || output_channels > 16u ||
        input_channels >= sizeof(size_t) * 8u || mode >= 3u) {
        return 0u;
    }

    b_count = (size_t)output_channels;
    m_count = (size_t)output_channels;
    a_count = (size_t)input_channels;
    point_count = (size_t)1u << input_channels;
    clut_values = point_count * (size_t)output_channels;

    b_offset = 32u;
    matrix_offset = icc_align4(b_offset + b_count * 12u);
    m_offset = icc_align4(matrix_offset + 48u);
    clut_offset = icc_align4(m_offset + m_count * 12u);
    a_offset = icc_align4(clut_offset + 20u + clut_values * 2u);
    total_length = a_offset + a_count * 12u;

    if (total_length > tag_capacity) {
        return 0u;
    }

    memset(tag, 0, total_length);
    memcpy(tag + 0u, "mAB ", 4u);
    tag[8u] = input_channels;
    tag[9u] = output_channels;
    icc_write_be32(tag + 12u, (uint32_t)b_offset);
    icc_write_be32(tag + 16u, (uint32_t)matrix_offset);
    icc_write_be32(tag + 20u, (uint32_t)m_offset);
    icc_write_be32(tag + 24u, (uint32_t)clut_offset);
    icc_write_be32(tag + 28u, (uint32_t)a_offset);

    icc_build_curve_block(tag, b_offset, b_count);
    icc_build_matrix_block(tag, matrix_offset);
    icc_build_curve_block(tag, m_offset, m_count);
    icc_build_clut_block_mode(tag,
                              clut_offset,
                              input_channels,
                              output_channels,
                              mode);
    icc_build_curve_block(tag, a_offset, a_count);

    return total_length;
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
icc_build_rgb_profile(unsigned char *profile,
                      size_t profile_capacity,
                      int include_a2b0,
                      int include_a2b1,
                      int include_a2b2,
                      int truncate_a2b1,
                      int with_matrix_trc,
                      size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    unsigned char a2b1_tag[4096];
    unsigned char a2b2_tag[4096];
    unsigned char xyz_tag[20];
    unsigned char trc_tag[12];
    icc_tag_ref_t tags[9];
    size_t tag_count;
    size_t len0;
    size_t len1;
    size_t len2;

    tag_count = 0u;
    len0 = 0u;
    len1 = 0u;
    len2 = 0u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || out_length == NULL) {
        return 0;
    }

    len0 = icc_build_mab_mode_tag(a2b0_tag, sizeof(a2b0_tag), 3u, 3u, 0u);
    len1 = icc_build_mab_mode_tag(a2b1_tag, sizeof(a2b1_tag), 3u, 3u, 1u);
    len2 = icc_build_mab_mode_tag(a2b2_tag, sizeof(a2b2_tag), 3u, 3u, 2u);
    if (len0 == 0u || len1 == 0u || len2 == 0u) {
        return 0;
    }
    if (truncate_a2b1 && len1 > 4u) {
        len1 -= 4u;
    }

    if (with_matrix_trc) {
        icc_build_xyz_tag(xyz_tag, FIXED_1_0, FIXED_0_0, FIXED_0_0);
        memcpy(tags[tag_count].signature, "rXYZ", 4u);
        tags[tag_count].data = xyz_tag;
        tags[tag_count].length = sizeof(xyz_tag);
        ++tag_count;

        icc_build_xyz_tag(xyz_tag, FIXED_0_0, FIXED_1_0, FIXED_0_0);
        memcpy(tags[tag_count].signature, "gXYZ", 4u);
        tags[tag_count].data = xyz_tag;
        tags[tag_count].length = sizeof(xyz_tag);
        ++tag_count;

        icc_build_xyz_tag(xyz_tag, FIXED_0_0, FIXED_0_0, FIXED_1_0);
        memcpy(tags[tag_count].signature, "bXYZ", 4u);
        tags[tag_count].data = xyz_tag;
        tags[tag_count].length = sizeof(xyz_tag);
        ++tag_count;

        (void)icc_build_curv_identity_tag(trc_tag);
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

    if (include_a2b0) {
        memcpy(tags[tag_count].signature, "A2B0", 4u);
        tags[tag_count].data = a2b0_tag;
        tags[tag_count].length = len0;
        ++tag_count;
    }
    if (include_a2b1) {
        memcpy(tags[tag_count].signature, "A2B1", 4u);
        tags[tag_count].data = a2b1_tag;
        tags[tag_count].length = len1;
        ++tag_count;
    }
    if (include_a2b2) {
        memcpy(tags[tag_count].signature, "A2B2", 4u);
        tags[tag_count].data = a2b2_tag;
        tags[tag_count].length = len2;
        ++tag_count;
    }

    return icc_pack_profile(profile,
                            profile_capacity,
                            "RGB ",
                            "XYZ ",
                            tags,
                            tag_count,
                            out_length);
}

static int
icc_build_gray_profile(unsigned char *profile,
                       size_t profile_capacity,
                       size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    unsigned char a2b1_tag[4096];
    unsigned char a2b2_tag[4096];
    icc_tag_ref_t tags[3];
    size_t len0;
    size_t len1;
    size_t len2;

    len0 = 0u;
    len1 = 0u;
    len2 = 0u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || out_length == NULL) {
        return 0;
    }

    len0 = icc_build_mab_mode_tag(a2b0_tag, sizeof(a2b0_tag), 1u, 3u, 0u);
    len1 = icc_build_mab_mode_tag(a2b1_tag, sizeof(a2b1_tag), 1u, 3u, 1u);
    len2 = icc_build_mab_mode_tag(a2b2_tag, sizeof(a2b2_tag), 1u, 3u, 2u);
    if (len0 == 0u || len1 == 0u || len2 == 0u) {
        return 0;
    }

    memcpy(tags[0].signature, "A2B0", 4u);
    tags[0].data = a2b0_tag;
    tags[0].length = len0;

    memcpy(tags[1].signature, "A2B1", 4u);
    tags[1].data = a2b1_tag;
    tags[1].length = len1;

    memcpy(tags[2].signature, "A2B2", 4u);
    tags[2].data = a2b2_tag;
    tags[2].length = len2;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "GRAY",
                            "XYZ ",
                            tags,
                            3u,
                            out_length);
}

static int
icc_build_cmyk_profile(unsigned char *profile,
                       size_t profile_capacity,
                       size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    unsigned char a2b1_tag[4096];
    unsigned char a2b2_tag[4096];
    icc_tag_ref_t tags[3];
    size_t len0;
    size_t len1;
    size_t len2;

    len0 = 0u;
    len1 = 0u;
    len2 = 0u;
    memset(tags, 0, sizeof(tags));
    if (profile == NULL || out_length == NULL) {
        return 0;
    }

    len0 = icc_build_mab_mode_tag(a2b0_tag, sizeof(a2b0_tag), 4u, 3u, 0u);
    len1 = icc_build_mab_mode_tag(a2b1_tag, sizeof(a2b1_tag), 4u, 3u, 1u);
    len2 = icc_build_mab_mode_tag(a2b2_tag, sizeof(a2b2_tag), 4u, 3u, 2u);
    if (len0 == 0u || len1 == 0u || len2 == 0u) {
        return 0;
    }

    memcpy(tags[0].signature, "A2B0", 4u);
    tags[0].data = a2b0_tag;
    tags[0].length = len0;

    memcpy(tags[1].signature, "A2B1", 4u);
    tags[1].data = a2b1_tag;
    tags[1].length = len1;

    memcpy(tags[2].signature, "A2B2", 4u);
    tags[2].data = a2b2_tag;
    tags[2].length = len2;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "CMYK",
                            "XYZ ",
                            tags,
                            3u,
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
run_slot_apply_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double rgb[3];
    double cmyk_rgb[3];
    float dst[3];
    unsigned char cmyk[4];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));
    memset(cmyk_rgb, 0, sizeof(cmyk_rgb));
    memset(dst, 0, sizeof(dst));
    memset(cmyk, 0, sizeof(cmyk));

    if (!icc_build_rgb_profile(profile,
                               sizeof(profile),
                               1,
                               1,
                               1,
                               0,
                               0,
                               &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }

    rgb[0] = 1.0;
    rgb[1] = 0.0;
    rgb[2] = 0.0;
    if (!sixel_icc_apply_rgb_triplet_unit_with_a2b_slot(rgb,
                                                         &parsed,
                                                         0u,
                                                         0) ||
        !expect_slot_channel(rgb, 0u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    rgb[0] = 1.0;
    rgb[1] = 0.0;
    rgb[2] = 0.0;
    if (!sixel_icc_apply_rgb_triplet_unit_with_a2b_slot(rgb,
                                                         &parsed,
                                                         1u,
                                                         0) ||
        !expect_slot_channel(rgb, 1u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    rgb[0] = 1.0;
    rgb[1] = 0.0;
    rgb[2] = 0.0;
    if (!sixel_icc_apply_rgb_triplet_unit_with_a2b_slot(rgb,
                                                         &parsed,
                                                         2u,
                                                         0) ||
        !expect_slot_channel(rgb, 2u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_gray_profile(profile, sizeof(profile), &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }

    rgb[0] = 1.0;
    rgb[1] = 1.0;
    rgb[2] = 1.0;
    if (!sixel_icc_apply_rgb_triplet_unit_with_a2b_slot(rgb,
                                                         &parsed,
                                                         1u,
                                                         0) ||
        !expect_slot_channel(rgb, 1u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_cmyk_profile(profile, sizeof(profile), &profile_length) ||
        !sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }

    cmyk[0] = 255u;
    cmyk[1] = 0u;
    cmyk[2] = 0u;
    cmyk[3] = 0u;
    if (!sixel_icc_apply_cmyk_u8_to_rgb_float32_with_a2b_slot(dst,
                                                               cmyk,
                                                               1u,
                                                               &parsed,
                                                               2u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    cmyk_rgb[0] = (double)dst[0];
    cmyk_rgb[1] = (double)dst[1];
    cmyk_rgb[2] = (double)dst[2];
    if (!expect_slot_channel(cmyk_rgb, 2u)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    return 1;
}

static int
check_transform_rgb8(sixel_cms_profile_t *src,
                     sixel_cms_profile_t *dst,
                     unsigned char const input[3],
                     unsigned int expected_slot)
{
    sixel_cms_transform_t *tr;
    unsigned char out[3];
    double rgb[3];

    tr = NULL;
    memset(out, 0, sizeof(out));
    memset(rgb, 0, sizeof(rgb));
    if (src == NULL || dst == NULL || input == NULL) {
        return 0;
    }

    tr = sixel_cms_create_transform(src,
                                    SIXEL_CMS_PIXELFORMAT_RGB_8,
                                    dst,
                                    SIXEL_CMS_PIXELFORMAT_RGB_8,
                                    SIXEL_CMS_TRANSFORM_DEFAULT);
    if (tr == NULL) {
        return 0;
    }
    if (!sixel_cms_do_transform(tr, input, out, 1u)) {
        sixel_cms_delete_transform(tr);
        return 0;
    }
    sixel_cms_delete_transform(tr);

    rgb[0] = (double)out[0] / 255.0;
    rgb[1] = (double)out[1] / 255.0;
    rgb[2] = (double)out[2] / 255.0;
    return expect_slot_channel(rgb, expected_slot);
}

static int
test_setenv(char const *name, char const *value)
{
#if defined(HAVE__PUTENV_S)
    return _putenv_s(name, value);
#elif defined(HAVE_SETENV)
    extern int setenv(char const *name, char const *value, int overwrite);

    return setenv(name, value, 1);
#else
    (void)name;
    (void)value;
    return -1;
#endif
}

static int
run_intent_cases(void)
{
    unsigned char profile_rgb[8192];
    unsigned char profile_fallback[8192];
    size_t profile_len_rgb;
    size_t profile_len_fallback;
    sixel_cms_profile_t *src;
    sixel_cms_profile_t *src_fallback;
    sixel_cms_profile_t *dst;
    unsigned char input[3];
    sixel_cms_engine_t old_engine;

    profile_len_rgb = 0u;
    profile_len_fallback = 0u;
    src = NULL;
    src_fallback = NULL;
    dst = NULL;
    input[0] = 255u;
    input[1] = 0u;
    input[2] = 0u;
    old_engine = sixel_cms_get_engine();

    if (test_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "") != 0 ||
        test_setenv("SIXEL_CMS_RENDERING_INTENT", "") != 0) {
        return 0;
    }

    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    if (!icc_build_rgb_profile(profile_rgb,
                               sizeof(profile_rgb),
                               1,
                               1,
                               1,
                               0,
                               0,
                               &profile_len_rgb)) {
        sixel_cms_set_engine(old_engine);
        return 0;
    }

    src = sixel_cms_open_profile_from_mem(profile_rgb, profile_len_rgb);
    dst = sixel_cms_create_srgb_profile();
    if (src == NULL || dst == NULL) {
        sixel_cms_close_profile(src);
        sixel_cms_close_profile(dst);
        sixel_cms_set_engine(old_engine);
        return 0;
    }

    if (test_setenv("SIXEL_CMS_RENDERING_INTENT", "perceptual!") != 0 ||
        !check_transform_rgb8(src, dst, input, 0u)) {
        goto fail;
    }
    if (test_setenv("SIXEL_CMS_RENDERING_INTENT", "relative!") != 0 ||
        !check_transform_rgb8(src, dst, input, 1u)) {
        goto fail;
    }
    if (test_setenv("SIXEL_CMS_RENDERING_INTENT", "absolute!") != 0 ||
        !check_transform_rgb8(src, dst, input, 1u)) {
        goto fail;
    }
    if (test_setenv("SIXEL_CMS_RENDERING_INTENT", "saturation!") != 0 ||
        !check_transform_rgb8(src, dst, input, 2u)) {
        goto fail;
    }

    if (!icc_build_rgb_profile(profile_fallback,
                               sizeof(profile_fallback),
                               1,
                               1,
                               1,
                               1,
                               0,
                               &profile_len_fallback)) {
        goto fail;
    }

    src_fallback = sixel_cms_open_profile_from_mem(profile_fallback,
                                                   profile_len_fallback);
    if (src_fallback == NULL) {
        goto fail;
    }

    if (test_setenv("SIXEL_CMS_RENDERING_INTENT",
                    "relative,saturation!") != 0 ||
        !check_transform_rgb8(src_fallback, dst, input, 2u)) {
        goto fail;
    }

    sixel_cms_close_profile(src_fallback);
    sixel_cms_close_profile(src);
    sixel_cms_close_profile(dst);
    (void)test_setenv("SIXEL_CMS_RENDERING_INTENT", "");
    sixel_cms_set_engine(old_engine);
    return 1;

fail:
    sixel_cms_close_profile(src_fallback);
    sixel_cms_close_profile(src);
    sixel_cms_close_profile(dst);
    (void)test_setenv("SIXEL_CMS_RENDERING_INTENT", "");
    sixel_cms_set_engine(old_engine);
    return 0;
}

int
test_icc_0003_icc_builtin_a2b_intent_paths(int argc, char **argv)
{
    int ok;

    (void)argc;
    (void)argv;

    ok = 1;
    if (!run_slot_apply_cases()) {
        fprintf(stderr, "A2B slot apply coverage cases failed\n");
        ok = 0;
    }
    if (!run_intent_cases()) {
        fprintf(stderr, "builtin intent to A2B slot cases failed\n");
        ok = 0;
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

#undef test_setenv
#undef check_transform_rgb8
#undef expect_slot_channel
#undef icc_build_cmyk_profile
#undef icc_build_gray_profile
#undef icc_build_rgb_profile
#undef icc_pack_profile
#undef icc_build_xyz_tag
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
