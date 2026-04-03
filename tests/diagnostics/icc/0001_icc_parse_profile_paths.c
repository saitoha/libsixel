/*
 * Cover ICC parser paths that do not require optional image loaders.
 *
 * The test builds compact synthetic ICC profiles in memory and verifies
 * that sixel_icc_parse_profile() accepts valid parametric/LUT16 profiles
 * and rejects malformed ones. It also exercises sixel_icc_parse_png_iccp()
 * failure paths with minimal synthetic PNG payloads.
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

#include "src/icc-parse.h"

#define ICC_PROFILE_HEADER_SIZE 128u
#define ICC_TAG_ENTRY_SIZE 12u
#define ICC_TAG_TABLE_BASE 128u
#define ICC_PARAM_TABLE_MIN 1024u

#define FIXED_0_0 0x00000000u
#define FIXED_0_02 0x0000051fu
#define FIXED_0_05 0x00000ccdu
#define FIXED_0_1 0x0000199au
#define FIXED_0_2 0x00003333u
#define FIXED_0_25 0x00004000u
#define FIXED_0_3 0x00004ccdu
#define FIXED_0_5 0x00008000u
#define FIXED_0_6 0x0000999au
#define FIXED_0_7 0x0000b333u
#define FIXED_1_0 0x00010000u
#define FIXED_2_0 0x00020000u
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
icc_build_para_tag(unsigned char *tag,
                   uint16_t function_type,
                   uint32_t const *params,
                   size_t param_count)
{
    size_t i;

    i = 0u;
    if (tag == NULL) {
        return 0u;
    }

    memcpy(tag + 0u, "para", 4u);
    memset(tag + 4u, 0, 4u);
    icc_write_be16(tag + 8u, function_type);
    memset(tag + 10u, 0, 2u);

    for (i = 0u; i < param_count; ++i) {
        icc_write_be32(tag + 12u + i * 4u, params[i]);
    }

    return 12u + param_count * 4u;
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
    size_t entry_offset;
    size_t data_offset;
    size_t i;

    table_size = 0u;
    entry_offset = 0u;
    data_offset = 0u;
    i = 0u;

    if (profile == NULL || tags == NULL || out_length == NULL) {
        return 0;
    }
    if (tag_count > (SIZE_MAX - 4u) / ICC_TAG_ENTRY_SIZE) {
        return 0;
    }

    table_size = 4u + tag_count * ICC_TAG_ENTRY_SIZE;
    if (ICC_TAG_TABLE_BASE > SIZE_MAX - table_size) {
        return 0;
    }

    data_offset = ICC_TAG_TABLE_BASE + table_size;
    if (data_offset > profile_capacity) {
        return 0;
    }

    memset(profile, 0, profile_capacity);
    memcpy(profile + 16u, color_space, 4u);
    memcpy(profile + 20u, pcs, 4u);
    icc_write_be32(profile + ICC_TAG_TABLE_BASE, (uint32_t)tag_count);

    for (i = 0u; i < tag_count; ++i) {
        if (tags[i].length > profile_capacity - data_offset) {
            return 0;
        }
        if (data_offset > UINT32_MAX || tags[i].length > UINT32_MAX) {
            return 0;
        }

        entry_offset = ICC_TAG_TABLE_BASE + 4u + i * ICC_TAG_ENTRY_SIZE;
        memcpy(profile + entry_offset, tags[i].signature, 4u);
        icc_write_be32(profile + entry_offset + 4u, (uint32_t)data_offset);
        icc_write_be32(profile + entry_offset + 8u, (uint32_t)tags[i].length);
        memcpy(profile + data_offset, tags[i].data, tags[i].length);
        data_offset += tags[i].length;
    }

    if (data_offset > UINT32_MAX) {
        return 0;
    }

    icc_write_be32(profile + 0u, (uint32_t)data_offset);
    *out_length = data_offset;
    return 1;
}

static int
icc_build_rgb_parametric_profile(unsigned char *profile,
                                 size_t profile_capacity,
                                 int variant,
                                 size_t *out_length)
{
    unsigned char rxyz_tag[20];
    unsigned char gxyz_tag[20];
    unsigned char bxyz_tag[20];
    unsigned char rtrc_tag[64];
    unsigned char gtrc_tag[64];
    unsigned char btrc_tag[64];
    uint32_t const *r_params;
    uint32_t const *g_params;
    uint32_t const *b_params;
    size_t r_count;
    size_t g_count;
    size_t b_count;
    uint16_t r_type;
    uint16_t g_type;
    uint16_t b_type;
    icc_tag_ref_t tags[6];
    static uint32_t const params_type0[] = { FIXED_2_2 };
    static uint32_t const params_type1[] = {
        FIXED_2_0, FIXED_1_0, FIXED_0_0
    };
    static uint32_t const params_type2[] = {
        FIXED_2_0, FIXED_1_0, FIXED_0_0, FIXED_0_1
    };
    static uint32_t const params_type3[] = {
        FIXED_2_0, FIXED_1_0, FIXED_0_0, FIXED_0_5, FIXED_0_25
    };
    static uint32_t const params_type4a[] = {
        FIXED_2_0, FIXED_1_0, FIXED_0_0, FIXED_0_5,
        FIXED_0_1, FIXED_0_25, FIXED_0_05
    };
    static uint32_t const params_type4b[] = {
        FIXED_2_2, FIXED_1_0, FIXED_0_0, FIXED_0_3,
        FIXED_0_0, FIXED_0_2, FIXED_0_02
    };
    static uint32_t const params_invalid_gamma[] = { FIXED_0_0 };

    r_params = NULL;
    g_params = NULL;
    b_params = NULL;
    r_count = 0u;
    g_count = 0u;
    b_count = 0u;
    r_type = 0u;
    g_type = 0u;
    b_type = 0u;

    icc_build_xyz_tag(rxyz_tag, FIXED_0_5, FIXED_0_2, FIXED_0_1);
    icc_build_xyz_tag(gxyz_tag, FIXED_0_3, FIXED_0_6, FIXED_0_1);
    icc_build_xyz_tag(bxyz_tag, FIXED_0_2, FIXED_0_1, FIXED_0_7);

    /*
     * Each variant selects a curve combination that maps to one parser path.
     * Variants 0/1 are valid and should build sampled tables.
     * Variant 2 uses unsupported function type and must fail.
     * Variant 3 uses gamma <= 0 and must fail.
     */
    switch (variant) {
    case 0:
        r_type = 0u;
        r_params = params_type0;
        r_count = sizeof(params_type0) / sizeof(params_type0[0]);
        g_type = 1u;
        g_params = params_type1;
        g_count = sizeof(params_type1) / sizeof(params_type1[0]);
        b_type = 2u;
        b_params = params_type2;
        b_count = sizeof(params_type2) / sizeof(params_type2[0]);
        break;
    case 1:
        r_type = 3u;
        r_params = params_type3;
        r_count = sizeof(params_type3) / sizeof(params_type3[0]);
        g_type = 4u;
        g_params = params_type4a;
        g_count = sizeof(params_type4a) / sizeof(params_type4a[0]);
        b_type = 4u;
        b_params = params_type4b;
        b_count = sizeof(params_type4b) / sizeof(params_type4b[0]);
        break;
    case 2:
        r_type = 5u;
        r_params = params_type0;
        r_count = sizeof(params_type0) / sizeof(params_type0[0]);
        g_type = 1u;
        g_params = params_type1;
        g_count = sizeof(params_type1) / sizeof(params_type1[0]);
        b_type = 2u;
        b_params = params_type2;
        b_count = sizeof(params_type2) / sizeof(params_type2[0]);
        break;
    case 3:
        r_type = 0u;
        r_params = params_invalid_gamma;
        r_count = sizeof(params_invalid_gamma)
            / sizeof(params_invalid_gamma[0]);
        g_type = 1u;
        g_params = params_type1;
        g_count = sizeof(params_type1) / sizeof(params_type1[0]);
        b_type = 2u;
        b_params = params_type2;
        b_count = sizeof(params_type2) / sizeof(params_type2[0]);
        break;
    default:
        return 0;
    }

    tags[0].signature[0] = 'r';
    tags[0].signature[1] = 'X';
    tags[0].signature[2] = 'Y';
    tags[0].signature[3] = 'Z';
    tags[0].data = rxyz_tag;
    tags[0].length = sizeof(rxyz_tag);

    tags[1].signature[0] = 'g';
    tags[1].signature[1] = 'X';
    tags[1].signature[2] = 'Y';
    tags[1].signature[3] = 'Z';
    tags[1].data = gxyz_tag;
    tags[1].length = sizeof(gxyz_tag);

    tags[2].signature[0] = 'b';
    tags[2].signature[1] = 'X';
    tags[2].signature[2] = 'Y';
    tags[2].signature[3] = 'Z';
    tags[2].data = bxyz_tag;
    tags[2].length = sizeof(bxyz_tag);

    tags[3].signature[0] = 'r';
    tags[3].signature[1] = 'T';
    tags[3].signature[2] = 'R';
    tags[3].signature[3] = 'C';
    tags[3].data = rtrc_tag;
    tags[3].length = icc_build_para_tag(rtrc_tag, r_type, r_params, r_count);

    tags[4].signature[0] = 'g';
    tags[4].signature[1] = 'T';
    tags[4].signature[2] = 'R';
    tags[4].signature[3] = 'C';
    tags[4].data = gtrc_tag;
    tags[4].length = icc_build_para_tag(gtrc_tag, g_type, g_params, g_count);

    tags[5].signature[0] = 'b';
    tags[5].signature[1] = 'T';
    tags[5].signature[2] = 'R';
    tags[5].signature[3] = 'C';
    tags[5].data = btrc_tag;
    tags[5].length = icc_build_para_tag(btrc_tag, b_type, b_params, b_count);

    return icc_pack_profile(profile,
                            profile_capacity,
                            "RGB ",
                            "XYZ ",
                            tags,
                            6u,
                            out_length);
}

static int
icc_build_cmyk_mft2_profile(unsigned char *profile,
                            size_t profile_capacity,
                            unsigned char output_channels,
                            int truncate_samples,
                            size_t *out_length)
{
    unsigned char a2b0_tag[256];
    icc_tag_ref_t tags[1];
    size_t input_count;
    size_t clut_count;
    size_t output_count;
    size_t sample_count;
    size_t full_length;
    size_t tag_length;
    size_t offset;
    size_t i;
    size_t point;
    size_t channel;
    uint16_t value;

    input_count = 0u;
    clut_count = 0u;
    output_count = 0u;
    sample_count = 0u;
    full_length = 0u;
    tag_length = 0u;
    offset = 0u;
    i = 0u;
    point = 0u;
    channel = 0u;
    value = 0u;

    input_count = 4u * 2u;
    clut_count = 16u * (size_t)output_channels;
    output_count = (size_t)output_channels * 2u;
    sample_count = input_count + clut_count + output_count;
    full_length = 52u + sample_count * 2u;

    if (full_length > sizeof(a2b0_tag)) {
        return 0;
    }

    memset(a2b0_tag, 0, sizeof(a2b0_tag));
    memcpy(a2b0_tag + 0u, "mft2", 4u);
    a2b0_tag[8u] = 4u;
    a2b0_tag[9u] = output_channels;
    a2b0_tag[10u] = 2u;
    icc_write_be16(a2b0_tag + 48u, 2u);
    icc_write_be16(a2b0_tag + 50u, 2u);

    /*
     * Input/output tables are monotonic endpoints; CLUT values are
     * synthetic.
     */
    offset = 52u;
    for (i = 0u; i < input_count; ++i) {
        value = ((i % 2u) == 0u) ? 0u : 65535u;
        icc_write_be16(a2b0_tag + offset, value);
        offset += 2u;
    }

    for (point = 0u; point < 16u; ++point) {
        for (channel = 0u; channel < (size_t)output_channels; ++channel) {
            value = (uint16_t)((point * 997u + channel * 193u) & 0xffffu);
            icc_write_be16(a2b0_tag + offset, value);
            offset += 2u;
        }
    }

    for (channel = 0u; channel < (size_t)output_channels; ++channel) {
        icc_write_be16(a2b0_tag + offset, 0u);
        offset += 2u;
        icc_write_be16(a2b0_tag + offset, 65535u);
        offset += 2u;
    }

    tag_length = full_length;
    if (truncate_samples && tag_length > 54u) {
        tag_length -= 2u;
    }

    tags[0].signature[0] = 'A';
    tags[0].signature[1] = '2';
    tags[0].signature[2] = 'B';
    tags[0].signature[3] = '0';
    tags[0].data = a2b0_tag;
    tags[0].length = tag_length;

    return icc_pack_profile(profile,
                            profile_capacity,
                            "CMYK",
                            "XYZ ",
                            tags,
                            1u,
                            out_length);
}

static int
icc_load_file(char const *path, unsigned char *buffer, size_t capacity,
              size_t *out_size)
{
    FILE *fp;
    size_t size;

    fp = NULL;
    size = 0u;
    if (path == NULL || buffer == NULL || out_size == NULL) {
        return 0;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }

    size = fread(buffer, 1u, capacity, fp);
    if (ferror(fp) != 0) {
        fclose(fp);
        return 0;
    }

    fclose(fp);
    *out_size = size;
    return 1;
}

static int
icc_build_png_iend_only(unsigned char *png, size_t capacity, size_t *out_size)
{
    static unsigned char const signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };

    if (png == NULL || out_size == NULL || capacity < 20u) {
        return 0;
    }

    memset(png, 0, capacity);
    memcpy(png, signature, sizeof(signature));
    icc_write_be32(png + 8u, 0u);
    memcpy(png + 12u, "IEND", 4u);
    *out_size = 20u;
    return 1;
}

static int
icc_build_png_bad_iccp(unsigned char *png, size_t capacity, size_t *out_size)
{
    static unsigned char const signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    unsigned char iccp_data[4];

    if (png == NULL || out_size == NULL || capacity < 36u) {
        return 0;
    }

    memset(png, 0, capacity);
    memcpy(png, signature, sizeof(signature));

    iccp_data[0] = 'x';
    iccp_data[1] = 0u;
    iccp_data[2] = 0u;
    iccp_data[3] = 0u;

    icc_write_be32(png + 8u, 4u);
    memcpy(png + 12u, "iCCP", 4u);
    memcpy(png + 16u, iccp_data, sizeof(iccp_data));

    icc_write_be32(png + 24u, 0u);
    memcpy(png + 28u, "IEND", 4u);

    *out_size = 36u;
    return 1;
}

static int
run_parametric_profile_cases(void)
{
    unsigned char profile[2048];
    size_t profile_length;
    sixel_icc_profile_t parsed;

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));

    if (!icc_build_rgb_parametric_profile(profile,
                                          sizeof(profile),
                                          0,
                                          &profile_length)) {
        return 0;
    }
    /* Cover function types 0/1/2 in one valid RGB profile. */
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_RGB ||
        parsed.pcs != SIXEL_ICC_PROFILE_PCS_XYZ ||
        parsed.curves[0].kind != SIXEL_ICC_CURVE_TABLE ||
        parsed.curves[1].kind != SIXEL_ICC_CURVE_TABLE ||
        parsed.curves[2].kind != SIXEL_ICC_CURVE_TABLE ||
        parsed.curves[0].table_length < ICC_PARAM_TABLE_MIN) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);
    memset(&parsed, 0, sizeof(parsed));

    if (!icc_build_rgb_parametric_profile(profile,
                                          sizeof(profile),
                                          1,
                                          &profile_length)) {
        return 0;
    }
    /* Cover function types 3/4 on valid parameters. */
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_RGB ||
        parsed.curves[0].table_length < ICC_PARAM_TABLE_MIN) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_rgb_parametric_profile(profile,
                                          sizeof(profile),
                                          2,
                                          &profile_length)) {
        return 0;
    }
    /* Unsupported function type must reject the profile. */
    if (sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    if (!icc_build_rgb_parametric_profile(profile,
                                          sizeof(profile),
                                          3,
                                          &profile_length)) {
        return 0;
    }
    /* Invalid gamma must reject the profile. */
    if (sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    return 1;
}

static int
run_lut16_profile_cases(void)
{
    unsigned char profile[1024];
    size_t profile_length;
    sixel_icc_profile_t parsed;

    profile_length = 0u;
    memset(&parsed, 0, sizeof(parsed));

    if (!icc_build_cmyk_mft2_profile(profile,
                                     sizeof(profile),
                                     3u,
                                     0,
                                     &profile_length)) {
        return 0;
    }
    /* Valid CMYK mft2 with 3-channel output should be accepted. */
    if (!sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        return 0;
    }
    if (parsed.kind != SIXEL_ICC_PROFILE_KIND_CMYK ||
        parsed.a2b0_lut.kind != SIXEL_ICC_LUT_MFT2 ||
        parsed.a2b0_lut.input_channels != 4u ||
        parsed.a2b0_lut.output_channels != 3u) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    if (!icc_build_cmyk_mft2_profile(profile,
                                     sizeof(profile),
                                     2u,
                                     0,
                                     &profile_length)) {
        return 0;
    }
    /* Output channel count mismatch must reject CMYK profiles. */
    if (sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    if (!icc_build_cmyk_mft2_profile(profile,
                                     sizeof(profile),
                                     3u,
                                     1,
                                     &profile_length)) {
        return 0;
    }
    /* Truncated sample payload must be rejected. */
    if (sixel_icc_parse_profile(profile, profile_length, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    return 1;
}

static int
run_png_iccp_cases(void)
{
    unsigned char png[512];
    unsigned char fixture[131072];
    unsigned char path_buffer[512];
    char const *top_srcdir;
    size_t png_size;
    size_t fixture_size;
    sixel_icc_profile_t parsed;

    top_srcdir = NULL;
    png_size = 0u;
    fixture_size = 0u;
    memset(&parsed, 0, sizeof(parsed));
    memset(path_buffer, 0, sizeof(path_buffer));

    if (!icc_build_png_iend_only(png, sizeof(png), &png_size)) {
        return 0;
    }
    /* PNG without iCCP chunk must return failure. */
    if (sixel_icc_parse_png_iccp(png, png_size, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    if (!icc_build_png_bad_iccp(png, sizeof(png), &png_size)) {
        return 0;
    }
    /* Invalid compressed iCCP payload must return failure. */
    if (sixel_icc_parse_png_iccp(png, png_size, &parsed)) {
        sixel_icc_profile_destroy(&parsed);
        return 0;
    }

    top_srcdir = getenv("TOP_SRCDIR");
    if (top_srcdir == NULL) {
        return 0;
    }
    if (snprintf((char *)path_buffer,
                 sizeof(path_buffer),
                 "%s/tests/data/colormgmt/input/png/rgb/"
                 "img_rgb_icc1_srgb0_chrm0_gama0.png",
                 top_srcdir) >= (int)sizeof(path_buffer)) {
        return 0;
    }

    if (!icc_load_file((char const *)path_buffer,
                       fixture,
                       sizeof(fixture),
                       &fixture_size)) {
        return 0;
    }
    /* Existing fixture with embedded profile must parse successfully. */
    if (!sixel_icc_parse_png_iccp(fixture, fixture_size, &parsed)) {
        return 0;
    }
    sixel_icc_profile_destroy(&parsed);

    return 1;
}

int
test_icc_0001_icc_parse_profile_paths(int argc, char **argv)
{
    int success;

    (void)argc;
    (void)argv;

    success = 1;

    if (!run_parametric_profile_cases()) {
        fprintf(stderr, "parametric ICC profile coverage cases failed\n");
        success = 0;
    }
    if (!run_lut16_profile_cases()) {
        fprintf(stderr, "LUT16 ICC profile coverage cases failed\n");
        success = 0;
    }
    if (!run_png_iccp_cases()) {
        fprintf(stderr, "PNG iCCP coverage cases failed\n");
        success = 0;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
