/*
 * Verify builtin ICC parser/apply paths for RGB/GRAY v4 extensions.
 *
 * This test synthesizes compact ICC profiles in memory so it can cover:
 * - RGB segmented TRC (segm) parsing and table sampling.
 * - RGB/GRAY A2B0 (mft1/mft2) parsing and apply behavior.
 * - Matrix/TRC fallback when A2B0 exists but is invalid.
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
#define FIXED_0_2 0x00003333u
#define FIXED_0_3 0x00004ccdu
#define FIXED_0_6 0x0000999au
#define FIXED_0_7 0x0000b333u
#define FIXED_1_8 0x0001ccceu
#define FIXED_2_2 0x00023333u

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

static size_t
icc_build_segm_tag(unsigned char *tag, int valid)
{
    uint16_t seg0_type;
    uint16_t seg1_type;

    seg0_type = 0u;
    seg1_type = valid ? 0u : 7u;
    if (tag == NULL) {
        return 0u;
    }

    memcpy(tag + 0u, "segm", 4u);
    memset(tag + 4u, 0, 4u);

    /* 2 segments with one breakpoint at x=0.5 */
    icc_write_be32(tag + 8u, 2u);
    icc_write_be32(tag + 12u, 0x00008000u);

    /* Segment 0: type 0, gamma 1.8 */
    icc_write_be16(tag + 16u, seg0_type);
    icc_write_be16(tag + 18u, 0u);
    icc_write_be32(tag + 20u, FIXED_1_8);

    /* Segment 1: type 0 gamma 2.2 or invalid type */
    icc_write_be16(tag + 24u, seg1_type);
    icc_write_be16(tag + 26u, 0u);
    icc_write_be32(tag + 28u, FIXED_2_2);

    return 32u;
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

static size_t
icc_build_mft2_identity(unsigned char *tag,
                        size_t tag_capacity,
                        uint8_t input_channels,
                        uint8_t output_channels)
{
    size_t clut_points;
    size_t input_count;
    size_t clut_count;
    size_t output_count;
    size_t length;
    size_t offset;
    size_t point;
    size_t channel;
    size_t shift;
    int bit;
    uint16_t value;

    clut_points = 0u;
    input_count = 0u;
    clut_count = 0u;
    output_count = 0u;
    length = 0u;
    offset = 0u;
    point = 0u;
    channel = 0u;
    shift = 0u;
    bit = 0;
    value = 0u;

    if (tag == NULL || input_channels == 0u || output_channels == 0u) {
        return 0u;
    }
    if (input_channels >= sizeof(size_t) * 8u) {
        return 0u;
    }

    clut_points = (size_t)1u << input_channels;
    input_count = (size_t)input_channels * 2u;
    clut_count = clut_points * (size_t)output_channels;
    output_count = (size_t)output_channels * 2u;
    length = 52u + (input_count + clut_count + output_count) * 2u;
    if (length > tag_capacity) {
        return 0u;
    }

    memset(tag, 0, length);
    memcpy(tag + 0u, "mft2", 4u);
    tag[8u] = input_channels;
    tag[9u] = output_channels;
    tag[10u] = 2u;
    icc_write_be16(tag + 48u, 2u);
    icc_write_be16(tag + 50u, 2u);

    offset = 52u;
    for (channel = 0u; channel < (size_t)input_channels; ++channel) {
        icc_write_be16(tag + offset, 0u);
        offset += 2u;
        icc_write_be16(tag + offset, 65535u);
        offset += 2u;
    }

    for (point = 0u; point < clut_points; ++point) {
        for (channel = 0u; channel < (size_t)output_channels; ++channel) {
            value = 0u;
            if (input_channels == 1u) {
                bit = (int)(point & 1u);
            } else {
                shift = (size_t)input_channels - 1u;
                if (channel < (size_t)input_channels) {
                    shift -= channel;
                } else {
                    shift = 0u;
                }
                bit = (int)((point >> shift) & 1u);
            }
            if (bit != 0) {
                value = 65535u;
            }
            icc_write_be16(tag + offset, value);
            offset += 2u;
        }
    }

    for (channel = 0u; channel < (size_t)output_channels; ++channel) {
        icc_write_be16(tag + offset, 0u);
        offset += 2u;
        icc_write_be16(tag + offset, 65535u);
        offset += 2u;
    }

    return length;
}

static size_t
icc_build_mft1_identity(unsigned char *tag,
                        size_t tag_capacity,
                        uint8_t input_channels,
                        uint8_t output_channels)
{
    size_t clut_points;
    size_t input_count;
    size_t clut_count;
    size_t output_count;
    size_t length;
    size_t offset;
    size_t i;
    size_t point;
    size_t channel;
    size_t shift;
    int bit;

    clut_points = 0u;
    input_count = 0u;
    clut_count = 0u;
    output_count = 0u;
    length = 0u;
    offset = 0u;
    i = 0u;
    point = 0u;
    channel = 0u;
    shift = 0u;
    bit = 0;

    if (tag == NULL || input_channels == 0u || output_channels == 0u) {
        return 0u;
    }
    if (input_channels >= sizeof(size_t) * 8u) {
        return 0u;
    }

    clut_points = (size_t)1u << input_channels;
    input_count = (size_t)input_channels * 256u;
    clut_count = clut_points * (size_t)output_channels;
    output_count = (size_t)output_channels * 256u;
    length = 48u + input_count + clut_count + output_count;
    if (length > tag_capacity) {
        return 0u;
    }

    memset(tag, 0, length);
    memcpy(tag + 0u, "mft1", 4u);
    tag[8u] = input_channels;
    tag[9u] = output_channels;
    tag[10u] = 2u;

    offset = 48u;
    for (i = 0u; i < input_count; ++i) {
        tag[offset + i] = (unsigned char)(i % 256u);
    }
    offset += input_count;

    for (point = 0u; point < clut_points; ++point) {
        for (channel = 0u; channel < (size_t)output_channels; ++channel) {
            if (input_channels == 1u) {
                bit = (int)(point & 1u);
            } else {
                shift = (size_t)input_channels - 1u;
                if (channel < (size_t)input_channels) {
                    shift -= channel;
                } else {
                    shift = 0u;
                }
                bit = (int)((point >> shift) & 1u);
            }
            tag[offset++] = (bit != 0) ? 255u : 0u;
        }
    }

    for (i = 0u; i < output_count; ++i) {
        tag[offset + i] = (unsigned char)(i % 256u);
    }

    return length;
}

static int
icc_build_rgb_segm_profile(unsigned char *profile,
                           size_t profile_capacity,
                           int valid_segm,
                           size_t *out_length)
{
    unsigned char rxyz_tag[20];
    unsigned char gxyz_tag[20];
    unsigned char bxyz_tag[20];
    unsigned char trc_tag[64];
    icc_tag_ref_t tags[6];
    size_t trc_length;

    trc_length = 0u;
    if (profile == NULL || out_length == NULL) {
        return 0;
    }

    icc_build_xyz_tag(rxyz_tag, FIXED_0_6, FIXED_0_3, FIXED_0_2);
    icc_build_xyz_tag(gxyz_tag, FIXED_0_3, FIXED_0_6, FIXED_0_2);
    icc_build_xyz_tag(bxyz_tag, FIXED_0_2, FIXED_0_3, FIXED_0_7);
    trc_length = icc_build_segm_tag(trc_tag, valid_segm);

    memcpy(tags[0].signature, "rXYZ", 4u);
    tags[0].data = rxyz_tag;
    tags[0].length = sizeof(rxyz_tag);

    memcpy(tags[1].signature, "gXYZ", 4u);
    tags[1].data = gxyz_tag;
    tags[1].length = sizeof(gxyz_tag);

    memcpy(tags[2].signature, "bXYZ", 4u);
    tags[2].data = bxyz_tag;
    tags[2].length = sizeof(bxyz_tag);

    memcpy(tags[3].signature, "rTRC", 4u);
    tags[3].data = trc_tag;
    tags[3].length = trc_length;

    memcpy(tags[4].signature, "gTRC", 4u);
    tags[4].data = trc_tag;
    tags[4].length = trc_length;

    memcpy(tags[5].signature, "bTRC", 4u);
    tags[5].data = trc_tag;
    tags[5].length = trc_length;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "RGB ",
                            "XYZ ",
                            tags,
                            6u,
                            out_length);
}

static int
icc_build_rgb_a2b0_profile(unsigned char *profile,
                           size_t profile_capacity,
                           int with_matrix_trc,
                           int force_bad_channels,
                           int pcs_lab,
                           size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    unsigned char xyz_tag[20];
    unsigned char trc_tag[16];
    icc_tag_ref_t tags[7];
    size_t a2b0_length;
    size_t tag_count;

    a2b0_length = 0u;
    tag_count = 0u;
    if (profile == NULL || out_length == NULL) {
        return 0;
    }

    a2b0_length = icc_build_mft2_identity(a2b0_tag,
                                          sizeof(a2b0_tag),
                                          3u,
                                          force_bad_channels ? 2u : 3u);
    if (a2b0_length == 0u) {
        return 0;
    }

    tag_count = 0u;
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
        tags[tag_count].length = 12u;
        ++tag_count;

        memcpy(tags[tag_count].signature, "gTRC", 4u);
        tags[tag_count].data = trc_tag;
        tags[tag_count].length = 12u;
        ++tag_count;

        memcpy(tags[tag_count].signature, "bTRC", 4u);
        tags[tag_count].data = trc_tag;
        tags[tag_count].length = 12u;
        ++tag_count;
    }

    memcpy(tags[tag_count].signature, "A2B0", 4u);
    tags[tag_count].data = a2b0_tag;
    tags[tag_count].length = a2b0_length;
    ++tag_count;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "RGB ",
                            pcs_lab ? "Lab " : "XYZ ",
                            tags,
                            tag_count,
                            out_length);
}

static int
icc_build_gray_a2b0_profile(unsigned char *profile,
                            size_t profile_capacity,
                            int force_bad_channels,
                            size_t *out_length)
{
    unsigned char a2b0_tag[4096];
    icc_tag_ref_t tags[1];
    size_t a2b0_length;

    a2b0_length = 0u;
    if (profile == NULL || out_length == NULL) {
        return 0;
    }

    a2b0_length = icc_build_mft1_identity(a2b0_tag,
                                          sizeof(a2b0_tag),
                                          1u,
                                          force_bad_channels ? 2u : 3u);
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
run_segm_curve_cases(void)
{
    unsigned char profile[4096];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double rgb[3];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));

    if (!icc_build_rgb_segm_profile(profile,
                                    sizeof(profile),
                                    1,
                                    &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_RGB ||
        parsed.curves[0].kind != SIXEL_ICC_CURVE_SEGM_TABLE ||
        parsed.curves[0].table_length < 1024u) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    rgb[0] = 0.3;
    rgb[1] = 0.5;
    rgb[2] = 0.8;
    if (!sixel_icc_apply_rgb_triplet_unit(rgb, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    if (rgb[0] < 0.0 || rgb[0] > 1.0 ||
        rgb[1] < 0.0 || rgb[1] > 1.0 ||
        rgb[2] < 0.0 || rgb[2] > 1.0) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_rgb_segm_profile(profile,
                                    sizeof(profile),
                                    0,
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
run_rgb_a2b0_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double rgb[3];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));

    if (!icc_build_rgb_a2b0_profile(profile,
                                    sizeof(profile),
                                    1,
                                    0,
                                    0,
                                    &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_RGB ||
        parsed.a2b0_lut.kind != SIXEL_ICC_LUT_MFT2_RGB_GRAY_A2B0 ||
        parsed.a2b0_lut.input_channels != 3u ||
        parsed.a2b0_lut.output_channels != 3u) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    /* A2B0 path should win over zero matrix/TRC fallback. */
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

    /* Lab PCS should be accepted when RGB A2B0 is available. */
    if (!icc_build_rgb_a2b0_profile(profile,
                                    sizeof(profile),
                                    0,
                                    0,
                                    1,
                                    &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.a2b0_lut.kind != SIXEL_ICC_LUT_MFT2_RGB_GRAY_A2B0) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    /* Invalid A2B0 must fail when no matrix/TRC fallback is available. */
    if (!icc_build_rgb_a2b0_profile(profile,
                                    sizeof(profile),
                                    0,
                                    1,
                                    0,
                                    &profile_length)) {
        return 0;
    }
    if (sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    /* Invalid A2B0 should fall back to matrix/TRC if those tags exist. */
    if (!icc_build_rgb_a2b0_profile(profile,
                                    sizeof(profile),
                                    1,
                                    1,
                                    0,
                                    &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.a2b0_lut.kind != SIXEL_ICC_LUT_INVALID ||
        parsed.curves[0].kind == SIXEL_ICC_CURVE_INVALID) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    return 1;
}

static int
run_gray_a2b0_cases(void)
{
    unsigned char profile[8192];
    size_t profile_length;
    sixel_icc_profile_t parsed;
    double rgb[3];

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));

    if (!icc_build_gray_a2b0_profile(profile,
                                     sizeof(profile),
                                     0,
                                     &profile_length)) {
        return 0;
    }
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_GRAY ||
        parsed.a2b0_lut.kind != SIXEL_ICC_LUT_MFT1_RGB_GRAY_A2B0 ||
        parsed.a2b0_lut.input_channels != 1u ||
        parsed.a2b0_lut.output_channels != 3u) {
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

    if (!icc_build_gray_a2b0_profile(profile,
                                     sizeof(profile),
                                     1,
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
test_icc_0001_icc_builtin_rgb_gray_v4_paths(int argc, char **argv)
{
    int success;

    (void)argc;
    (void)argv;

    success = 1;
    if (!run_segm_curve_cases()) {
        fprintf(stderr, "segm parser/apply coverage cases failed\n");
        success = 0;
    }
    if (!run_rgb_a2b0_cases()) {
        fprintf(stderr, "RGB A2B0 coverage cases failed\n");
        success = 0;
    }
    if (!run_gray_a2b0_cases()) {
        fprintf(stderr, "GRAY A2B0 coverage cases failed\n");
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
