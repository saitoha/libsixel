/* SPDX-License-Identifier: MIT */
/* Finite fixed-point coefficients can still produce NaN/Inf in pow().
 * Profiles and a one-pixel BMP are constructed in memory, without a CMM
 * oracle or fixture I/O. Every caller reports one independent observation. */
#include <math.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "tests/_support/diagnostics/icc_numeric_common.h"
#include "tests/data/color-numeric/icc.h"
#include "src/icc-parse.h"
#include "src/cms.h"
#include "src/loader-common.h"

static void
iccn_be32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void
iccn_le32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void
iccn_reverse_sample(unsigned char **p, int lut16, unsigned int sample)
{
    *(*p)++ = (unsigned char)sample;
    if (lut16) {
        /* This fixture uses only 0 and 65535 in the word tables. */
        *(*p)++ = (unsigned char)sample;
    }
}

int
icc_numeric_reverse_matrix_test(int lut16, char const *tag_name)
{
    static unsigned int const matrix[] = {
        0u, 0u, 0x00010000u,
        0x00010000u, 0xffff8000u, 0x00008000u,
        0x00010000u, 0x00010000u, 0x00010000u
    };
    static double const expected[] = {0.25, 0.375, 0.0};
    unsigned char bytes[2048];
    unsigned char *forward;
    unsigned char *reverse;
    unsigned char *p;
    sixel_color_def_t *definition;
    double xyz[3];
    double rgb[3];
    double scale;
    size_t entries;
    size_t channel;
    size_t i;
    unsigned int sample;
    int ok;

    memset(bytes, 0, sizeof(bytes));
    bytes[8] = 4u;
    memcpy(bytes + 12u, "mntrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 128u, 2u);
    memcpy(bytes + 132u, "A2B0", 4u);
    iccn_be32(bytes + 136u, 156u);
    iccn_be32(bytes + 140u, 68u);
    memcpy(bytes + 144u, tag_name, 4u);
    iccn_be32(bytes + 148u, 224u);

    /* A B-curve-only forward tag satisfies the RGB parser. The test
     * passes known XYZ directly, so it cannot hide reverse errors. */
    forward = bytes + 156u;
    memcpy(forward, "mAB ", 4u);
    forward[8] = forward[9] = 3u;
    iccn_be32(forward + 12u, 32u);
    for (i = 0u; i < 3u; ++i) {
        memcpy(forward + 32u + i * 12u, "curv", 4u);
    }

    reverse = bytes + 224u;
    memcpy(reverse, lut16 ? "mft2" : "mft1", 4u);
    reverse[8] = reverse[9] = 3u;
    reverse[10] = 2u;
    for (i = 0u; i < 9u; ++i) {
        iccn_be32(reverse + 12u + i * 4u, matrix[i]);
    }
    entries = lut16 ? 2u : 256u;
    if (lut16) {
        reverse[49] = reverse[51] = 2u;
    }
    p = reverse + (lut16 ? 52u : 48u);
    for (channel = 0u; channel < 3u; ++channel) {
        for (i = 0u; i < entries; ++i) {
            sample = (unsigned int)(i * 255u / (entries - 1u));
            iccn_reverse_sample(&p, lut16,
                                channel == 1u ? sample : 255u - sample);
        }
    }
    for (i = 0u; i < 8u; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            iccn_reverse_sample(&p, lut16,
                                (i & (4u >> channel)) ? 255u : 0u);
        }
    }
    for (channel = 0u; channel < 3u; ++channel) {
        for (i = 0u; i < entries; ++i) {
            iccn_reverse_sample(&p, lut16,
                                (unsigned int)(i * 255u / (entries - 1u)));
        }
    }
    iccn_be32(bytes, (unsigned int)(p - bytes));
    iccn_be32(bytes + 152u, (unsigned int)(p - reverse));

    /* Physical XYZ maps to (1/4, 1/2, 3/4) in the LUT domain. The
     * matrix gives (3/4, 3/8, 3/2). The tables invert channels 0 and 2
     * after clipping, giving (1/4, 3/8, 0); the remaining stages are
     * identity. Different input curves make stage order observable. */
    scale = lut16 ? 65535.0 / 32768.0 : 255.0 / 128.0;
    xyz[0] = scale * 0.25;
    xyz[1] = scale * 0.5;
    xyz[2] = scale * 0.75;
    definition = sixel_cdef_icc(bytes, (size_t)(p - bytes));
    ok = sixel_cdef_from_xyz(definition, xyz, rgb, 0);
    if (ok) {
        for (channel = 0u; channel < 3u; ++channel) {
            if (!(fabs(rgb[channel] - expected[channel]) <= 1e-12)) {
                fprintf(stderr, "Channel %zu: expected %.17g, got %.17g\n",
                        channel, expected[channel], rgb[channel]);
                ok = 0;
            }
        }
    }
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static unsigned int
iccn_read32(unsigned char const *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
        ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

int
icc_numeric_tag_budget_test(int over_limit)
{
    unsigned char bytes[132u + 257u * 12u + 68u];
    unsigned char *entry;
    unsigned char *payload;
    size_t length;
    size_t offset;
    unsigned int tags;
    unsigned int i;
    sixel_cms_profile_t *profile;
    sixel_cms_profile_result_t report;
    sixel_cms_engine_t previous;
    int ok;

    tags = over_limit ? 257u : 256u;
    offset = 132u + (size_t)tags * 12u;
    length = offset + 68u;
    memset(bytes, 0, length);
    iccn_be32(bytes, (unsigned int)length);
    bytes[8] = 4u;
    memcpy(bytes + 12u, "mntrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 68u, 63190u);
    iccn_be32(bytes + 72u, 65536u);
    iccn_be32(bytes + 76u, 54061u);
    iccn_be32(bytes + 128u, tags);

    /* Each entry is in bounds. Only the last entry supplies the path. */
    for (i = 0u; i < tags; ++i) {
        entry = bytes + 132u + (size_t)i * 12u;
        memcpy(entry, "zzAA", 4u);
        entry[2] = (unsigned char)('A' + i / 26u);
        entry[3] = (unsigned char)('A' + i % 26u);
        iccn_be32(entry + 4u, (unsigned int)offset);
        iccn_be32(entry + 8u, 68u);
    }
    memcpy(bytes + offset - 12u, "A2B0", 4u);
    payload = bytes + offset;
    memcpy(payload, "mAB ", 4u);
    payload[8] = payload[9] = 3u;
    iccn_be32(payload + 12u, 32u);
    for (i = 0u; i < 3u; ++i) {
        memcpy(payload + 32u + i * 12u, "curv", 4u);
    }

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    profile = sixel_cms_open_profile_from_mem_ex(bytes, length, &report);
    if (over_limit) {
        ok = profile == NULL && report.actual_engine == SIXEL_CMS_ENGINE_NONE
            && report.status == SIXEL_CMS_STATUS_RESOURCE_LIMIT;
    } else {
        ok = profile != NULL && report.status == SIXEL_CMS_STATUS_OK &&
            report.skipped_path == SIXEL_CMS_STATUS_OK;
    }
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Two A2B slots share one 8-bit payload. Each owned copy uses
 * 2 * (128 * 128 * 85 * 3 + 16384) bytes, exactly 8 MiB.
 * Adding one curve sample to the shared payload charges four more bytes. */
#define ICCN_BUDGET_CLUT_VALUES ((size_t)128u * 128u * 85u * 3u)
#define ICCN_BUDGET_FIXTURE_BYTES \
    (156u + 32u + 12u + 2u * 16385u + 2u + 24u + 20u + ICCN_BUDGET_CLUT_VALUES)

static size_t
iccn_make_budget_profile(unsigned char *bytes, int over_limit)
{
    unsigned char *payload;
    size_t curve_entries;
    size_t identity_offset;
    size_t clut_offset;
    size_t tag_size;
    size_t profile_size;

    curve_entries = over_limit ? 16385u : 16384u;
    identity_offset = (32u + 12u + 2u * curve_entries + 3u) & ~(size_t)3u;
    clut_offset = identity_offset + 24u;
    tag_size = clut_offset + 20u + ICCN_BUDGET_CLUT_VALUES;
    profile_size = 156u + tag_size;
    memset(bytes, 0, profile_size);
    iccn_be32(bytes, (unsigned int)profile_size);
    bytes[8] = 4u;
    memcpy(bytes + 12u, "mntrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 128u, 2u);
    memcpy(bytes + 132u, "A2B0", 4u);
    iccn_be32(bytes + 136u, 156u);
    iccn_be32(bytes + 140u, (unsigned int)tag_size);
    memcpy(bytes + 144u, "A2B1", 4u);
    iccn_be32(bytes + 148u, 156u);
    iccn_be32(bytes + 152u, (unsigned int)tag_size);

    payload = bytes + 156u;
    memcpy(payload, "mAB ", 4u);
    payload[8] = payload[9] = 3u;
    iccn_be32(payload + 12u, 32u);
    iccn_be32(payload + 24u, (unsigned int)clut_offset);
    memcpy(payload + 32u, "curv", 4u);
    iccn_be32(payload + 40u, (unsigned int)curve_entries);
    memcpy(payload + identity_offset, "curv", 4u);
    memcpy(payload + identity_offset + 12u, "curv", 4u);
    payload[clut_offset] = 128u;
    payload[clut_offset + 1u] = 128u;
    payload[clut_offset + 2u] = 85u;
    payload[clut_offset + 16u] = 1u;
    return profile_size;
}

int
icc_numeric_parse_budget_test(int over_limit)
{
    unsigned char *bytes;
    size_t length;
    sixel_icc_profile_t profile;
    unsigned char untouched[sizeof(profile)];
    sixel_icc_issue_t issue;
    int parsed;
    int ok;

    bytes = (unsigned char *)malloc(ICCN_BUDGET_FIXTURE_BYTES);
    if (bytes == NULL) {
        return EXIT_FAILURE;
    }
    ok = 1;
    if (over_limit) {
        length = iccn_make_budget_profile(bytes, 1);
        memset(&profile, 0xa5, sizeof(profile));
        memcpy(untouched, &profile, sizeof(untouched));
        parsed = sixel_icc_parse_profile_ex(bytes, length, &profile, &issue);
        ok = !parsed && issue == SIXEL_ICC_ISSUE_RESOURCE_LIMIT &&
            memcmp(&profile, untouched, sizeof(profile)) == 0;
        if (parsed) {
            sixel_icc_profile_destroy(&profile);
        }
    }
    if (ok) {
        /* Retry at the exact limit using the same destination storage. */
        length = iccn_make_budget_profile(bytes, 0);
        parsed = sixel_icc_parse_profile_ex(bytes, length, &profile, &issue);
        ok = parsed && issue == SIXEL_ICC_ISSUE_NONE &&
            profile.a2b_mab[0].clut.value_count == ICCN_BUDGET_CLUT_VALUES &&
            profile.a2b_mab[1].clut.value_count == ICCN_BUDGET_CLUT_VALUES &&
            profile.a2b_mab[0].b_curves[0].table_length == 16384u &&
            profile.a2b_mab[1].b_curves[0].table_length == 16384u;
        if (parsed) {
            sixel_icc_profile_destroy(&profile);
        }
    }
    free(bytes);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_profile_status_test(int malformed, int keep_matrix)
{
    unsigned char bytes[sizeof(color_icc) + 20u];
    unsigned char *entry;
    unsigned char *payload;
    size_t count;
    size_t i;
    sixel_cms_profile_t *profile;
    sixel_cms_profile_result_t report;
    sixel_cms_engine_t previous;
    sixel_cms_status_t expected;
    int ok;

    memcpy(bytes, color_icc, sizeof(color_icc));
    payload = bytes + sizeof(color_icc);
    memset(payload, 0, 20u);
    memcpy(payload, malformed ? "mft2" : "mpet", 4u);
    iccn_be32(bytes, sizeof(bytes));
    count = iccn_read32(bytes + 128u);
    entry = bytes + 132u;
    if (keep_matrix) {
        /* Replace an optional tag so the colorants and TRCs can still win. */
        for (i = 0u; i < count && memcmp(entry, "chad", 4u) != 0; ++i) {
            entry += 12u;
        }
        if (i == count) {
            return EXIT_FAILURE;
        }
    } else {
        iccn_be32(bytes + 128u, 1u);
    }
    memcpy(entry, "D2B0", 4u);
    iccn_be32(entry + 4u, sizeof(color_icc));
    iccn_be32(entry + 8u, 20u);

    expected = malformed ? SIXEL_CMS_STATUS_MALFORMED_PROFILE
                         : SIXEL_CMS_STATUS_UNSUPPORTED_PROFILE;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    profile = sixel_cms_open_profile_from_mem_ex(bytes, sizeof(bytes),
                                                 &report);
    ok = report.first_engine == SIXEL_CMS_ENGINE_BUILTIN &&
        report.first_engine_failure == SIXEL_CMS_STATUS_OK;
    if (keep_matrix) {
        ok = ok && profile != NULL && report.status == SIXEL_CMS_STATUS_OK &&
            report.actual_engine == SIXEL_CMS_ENGINE_BUILTIN &&
            report.skipped_path == expected;
    } else {
        ok = ok && profile == NULL && report.status == expected &&
            report.actual_engine == SIXEL_CMS_ENGINE_NONE &&
            report.skipped_path == SIXEL_CMS_STATUS_OK;
    }
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_native_fallback_status_test(void)
{
    unsigned char bytes[sizeof(color_icc)];
    sixel_cms_profile_t *profile;
    sixel_cms_profile_result_t report;
    sixel_cms_engine_t previous;
    sixel_cms_engine_t preferred;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_LCMS2);
    preferred = sixel_cms_get_engine();
    if (preferred != SIXEL_CMS_ENGINE_LCMS2) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    /* Native engines check the ICC signature. Builtin can still evaluate
     * the same colorants and curves under its compatibility policy. */
    memcpy(bytes, color_icc, sizeof(bytes));
    memset(bytes + 36u, 0, 4u);
    profile = sixel_cms_open_profile_from_mem_ex(bytes, sizeof(bytes),
                                                 &report);
    ok = profile != NULL && report.status == SIXEL_CMS_STATUS_OK &&
        report.first_engine == preferred &&
        report.actual_engine == SIXEL_CMS_ENGINE_BUILTIN &&
        report.first_engine_failure == SIXEL_CMS_STATUS_BACKEND_REJECTED;
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_apply_status_test(void)
{
    static float const input[3] = {0.25f, 0.5f, 0.75f};
    float output[3];
    sixel_cms_profile_t *profile;
    sixel_cms_transform_t *transform;
    sixel_cms_engine_t previous;
    sixel_cms_status_t status;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    profile = sixel_cms_create_srgb_profile();
    transform = sixel_cms_create_transform_ex(profile,
        SIXEL_CMS_PIXELFORMAT_GRAY_8, profile, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        SIXEL_CMS_TRANSFORM_DEFAULT, &status);
    memcpy(output, input, sizeof(output));
    ok = transform != NULL && status == SIXEL_CMS_STATUS_OK &&
        !sixel_cms_do_transform_ex(transform, input, output, 1u, &status) &&
        status == SIXEL_CMS_STATUS_INCOMPATIBLE_FORMAT &&
        memcmp(output, input, sizeof(output)) == 0;
    sixel_cms_delete_transform(transform);
    if (ok) {
        transform = sixel_cms_create_transform_ex(
            profile, SIXEL_CMS_PIXELFORMAT_RGB_F32,
            profile, SIXEL_CMS_PIXELFORMAT_RGB_F32,
            SIXEL_CMS_TRANSFORM_DEFAULT, &status);
        ok = transform != NULL && status == SIXEL_CMS_STATUS_OK &&
            sixel_cms_do_transform_ex(transform, input, output, 1u, &status) &&
            status == SIXEL_CMS_STATUS_OK;
        sixel_cms_delete_transform(transform);
    }
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void
iccn_absolute_source(unsigned char bytes[sizeof(color_icc)])
{
    size_t i;
    size_t count;
    unsigned char const *entry;
    unsigned char *white;

    memcpy(bytes, color_icc, sizeof(color_icc));
    /* The P3 colorants already incorporate chad. Give the input profile
     * a medium whose white is dimmer and has a different chromaticity. */
    memcpy(bytes + 12u, "scnr", 4u);
    count = iccn_read32(bytes + 128u);
    for (i = 0u; i < count; ++i) {
        entry = bytes + 132u + i * 12u;
        if (memcmp(entry, "wtpt", 4u) == 0) {
            white = bytes + iccn_read32(entry + 4u) + 8u;
            iccn_be32(white, 57344u);
            iccn_be32(white + 4u, 49152u);
            iccn_be32(white + 8u, 40960u);
            return;
        }
    }
}

static int
iccn_check_absolute_pair(sixel_cms_profile_t *source,
                         sixel_cms_profile_t *destination,
                         float const input[3], double const expected[2][3],
                         char const *const intents[2])
{
    sixel_cms_transform_t *transforms[2];
    float output[3];
    size_t intent;
    size_t channel;
    int ok;

    memset(transforms, 0, sizeof(transforms));
    ok = source != NULL && destination != NULL;
    for (intent = 0u; ok && intent < 2u; ++intent) {
        if (sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT",
                                intents[intent]) != 0) {
            ok = 0;
            break;
        }
        transforms[intent] = sixel_cms_create_transform(source,
            SIXEL_CMS_PIXELFORMAT_RGB_F32, destination,
            SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
        ok = transforms[intent] != NULL;
    }
    /* Both descriptors must remember the choice made at creation. */
    for (intent = 0u; ok && intent < 2u; ++intent) {
        ok = sixel_cms_do_transform(transforms[intent], input, output, 1u);
        for (channel = 0u; ok && channel < 3u; ++channel) {
            if (!(fabs(output[channel] - expected[intent][channel]) < 2e-7)) {
                fprintf(stderr, "%s channel %zu: expected %.9g, got %.9g\n",
                        intents[intent], channel, expected[intent][channel],
                        (double)output[channel]);
                ok = 0;
            }
        }
    }
    sixel_cms_delete_transform(transforms[1]);
    sixel_cms_delete_transform(transforms[0]);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static char const *const iccn_absolute_intents[] = {
    "relative,absolute!", "absolute!"
};

int
icc_numeric_absolute_matrix_test(void)
{
    static float const input[3] = {0.25f, 0.5f, 0.75f};
    /* From the stored P3 para/TRCs and adapted colorant columns. For
     * absolute, multiply XYZ by (7/8, 3/4, 5/8) / D50. The final matrix
     * is the inverse of the sRGB basis derived from xy and Bradford. */
    static double const expected[2][3] = {
        {0.014169133463280738, 0.220915626153715852, 0.556046604618495345},
        {0.093356213137581713, 0.139467338091442965, 0.423482751109502806}
    };
    unsigned char bytes[sizeof(color_icc)];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_engine_t previous;
    int result;

    iccn_absolute_source(bytes);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(bytes, sizeof(bytes));
    destination = sixel_cms_create_linear_srgb_profile();
    result = iccn_check_absolute_pair(source, destination, input, expected,
                                       iccn_absolute_intents);
    sixel_cms_close_profile(destination);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return result;
}

static void
iccn_absolute_lut_profile(unsigned char bytes[312],
                          unsigned int const white[3], unsigned char slot)
{
    unsigned char *lut;
    unsigned char *p;
    size_t i;
    size_t channel;

    memset(bytes, 0, 312u);
    iccn_be32(bytes, 312u);
    bytes[8] = 4u;
    memcpy(bytes + 12u, "prtrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 68u, 63190u);
    iccn_be32(bytes + 72u, 65536u);
    iccn_be32(bytes + 76u, 54061u);
    iccn_be32(bytes + 128u, 3u);
    memcpy(bytes + 132u, "A2B1", 4u);
    bytes[135] = slot;
    iccn_be32(bytes + 136u, 168u);
    iccn_be32(bytes + 140u, 124u);
    memcpy(bytes + 144u, "B2A1", 4u);
    bytes[147] = slot;
    iccn_be32(bytes + 148u, 168u);
    iccn_be32(bytes + 152u, 124u);
    memcpy(bytes + 156u, "wtpt", 4u);
    iccn_be32(bytes + 160u, 292u);
    iccn_be32(bytes + 164u, 20u);
    memcpy(bytes + 292u, "XYZ ", 4u);
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(bytes + 300u + i * 4u, white[i]);
    }

    lut = bytes + 168u;
    memcpy(lut, "mft2", 4u);
    lut[8] = lut[9] = 3u;
    lut[10] = 2u;
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(lut + 12u + i * 16u, 65536u);
    }
    lut[49] = lut[51] = 2u;
    p = lut + 52u;
    for (i = 0u; i < 3u; ++i) {
        iccn_reverse_sample(&p, 1, 0u);
        iccn_reverse_sample(&p, 1, 255u);
    }
    for (i = 0u; i < 8u; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            iccn_reverse_sample(&p, 1, (i & (4u >> channel)) ? 255u : 0u);
        }
    }
    for (i = 0u; i < 3u; ++i) {
        iccn_reverse_sample(&p, 1, 0u);
        iccn_reverse_sample(&p, 1, 255u);
    }
}

int
icc_numeric_absolute_lut_test(int fallback)
{
    static unsigned int const src_white[] = {49152u, 57344u, 65536u};
    static unsigned int const dst_white[] = {65536u, 49152u, 32768u};
    static float const input[] = {0.5f, 0.375f, 0.25f};
    /* Both tables are identity. The XYZ encoding factors cancel, leaving
     * exactly the media ratio (3/4, 7/6, 2) for absolute. */
    static double const expected[2][3] = {
        {0.5, 0.375, 0.25}, {0.375, 0.4375, 0.5}
    };
    static double const expected_fallback[2][3] = {
        {0.5, 0.375, 0.25}, {0.5, 0.375, 0.25}
    };
    static char const *const fallback_intents[] = {
        "saturation!", "absolute,saturation!"
    };
    unsigned char src_bytes[312];
    unsigned char dst_bytes[312];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_engine_t previous;
    unsigned char slot;
    int result;

    /* Saturation-only profiles cannot supply the colorimetric basis. */
    slot = fallback ? '2' : '1';
    iccn_absolute_lut_profile(src_bytes, src_white, slot);
    iccn_absolute_lut_profile(dst_bytes, dst_white, slot);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(src_bytes, sizeof(src_bytes));
    destination = sixel_cms_open_profile_from_mem(dst_bytes, sizeof(dst_bytes));
    result = iccn_check_absolute_pair(source, destination, input,
        fallback ? expected_fallback : expected,
        fallback ? fallback_intents : iccn_absolute_intents);
    sixel_cms_close_profile(destination);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return result;
}

int
icc_numeric_absolute_definition_test(void)
{
    static double const input[] = {0.25, 0.5, 0.75};
    /* Independent evaluation of the stored type-4 curves and XYZ columns,
     * with the PCS-relative and media-white-scaled outputs in that order. */
    static double const expected[2][3] = {
        {0.170795938812887680, 0.195230144127393545, 0.418609569483359935},
        {0.154995277391906991, 0.146422608095545159, 0.317166906203297320}
    };
    unsigned char bytes[sizeof(color_icc)];
    sixel_color_def_t *definition;
    double xyz[3];
    double rgb[3];
    size_t i;
    size_t channel;
    int intent;
    int ok;

    iccn_absolute_source(bytes);
    definition = sixel_cdef_icc(bytes, sizeof(bytes));
    ok = definition != NULL;
    for (i = 0u; ok && i < 2u; ++i) {
        intent = i == 0u ? SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC
                         : SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC;
        ok = sixel_cdef_to_xyz(definition, input, xyz, intent) &&
            sixel_cdef_from_xyz(definition, expected[i], rgb, intent);
        for (channel = 0u; ok && channel < 3u; ++channel) {
            ok = fabs(xyz[channel] - expected[i][channel]) < 2e-14 &&
                fabs(rgb[channel] - input[channel]) < 2e-14;
        }
    }
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static size_t
iccn_profile(unsigned char *bytes, int segmented, int failure)
{
    unsigned char *tag;
    unsigned char *params;
    size_t i;
    size_t size;
    unsigned int count;

    memcpy(bytes, color_icc, sizeof(color_icc));
    tag = bytes + sizeof(color_icc);
    memset(tag, 0, 56u);
    size = sizeof(color_icc) + 56u;
    iccn_be32(bytes, (unsigned int)size);
    count = iccn_read32(bytes + 128);
    for (i = 0u; i < count; ++i) {
        /* Invalid gTRC follows a valid allocated rTRC, exercising cleanup.
         * The finite control replaces every TRC with the same exact curve. */
        if (memcmp(bytes + 132u + i * 12u, "gTRC", 4u) == 0 ||
            (!failure && memcmp(bytes + 133u + i * 12u, "TRC", 3u) == 0)) {
            iccn_be32(bytes + 136u + i * 12u, sizeof(color_icc));
            iccn_be32(bytes + 140u + i * 12u, 56u);
        }
    }
    if (segmented) {
        memcpy(tag, "segm", 4u);
        iccn_be32(tag + 8, failure ? 2u : 1u);
        params = tag + 16;
        if (failure) {
            /* The first half is identity; fail only in the second segment. */
            iccn_be32(tag + 12, 0x00008000u);
            iccn_be32(tag + 20, 0x00010000u);
            tag[25] = 4u;
            params = tag + 28;
        } else {
            tag[13] = 4u;
        }
    } else {
        memcpy(tag, "para", 4u);
        tag[9] = 4u;
        params = tag + 12;
    }
    /* Type 4: g,a,b,c,d,e,f. Finite control is clamp(4*x-1).
     * NaN: negative power base with exponent 1.5.
     * Inf: an exponent of 32767 overflows after x passes the knee. */
    iccn_be32(params, failure == 1 ? 0x00018000u :
        failure == 2 ? 0x7fff0000u : 0x00010000u);
    iccn_be32(params + 4, failure == 1 ? 0x00010000u :
        failure == 2 ? 0x00020000u : 0x00040000u);
    iccn_be32(params + 8, failure == 1 ? 0xfffe0000u :
        failure == 2 ? 0u : 0xffff0000u);
    iccn_be32(params + 12, 0x00010000u);
    iccn_be32(params + 16, failure ? 0x00008000u : 0u);
    return size;
}

int
icc_numeric_parse_test(int segmented, int failure)
{
    unsigned char bytes[sizeof(color_icc) + 56u];
    sixel_icc_profile_t profile;
    unsigned char untouched[sizeof(profile)];
    size_t size;
    size_t i;
    size_t n;
    long numerator;
    unsigned int expected;
    int c;
    int ok;

    size = iccn_profile(bytes, segmented, failure);
    memset(&profile, 0xa5, sizeof(profile));
    memcpy(untouched, &profile, sizeof(profile));
    ok = sixel_icc_parse_profile(bytes, size, &profile);
    if (failure) {
        if (ok) {
            sixel_icc_profile_destroy(&profile);
            return EXIT_FAILURE;
        }
        /* Failed parsing must not publish a partially initialized object. */
        if (memcmp(untouched, &profile, sizeof(profile)) != 0) {
            return EXIT_FAILURE;
        }
        if (!sixel_icc_parse_profile(color_icc, sizeof(color_icc), &profile)) {
            return EXIT_FAILURE;
        }
        sixel_icc_profile_destroy(&profile);
        return EXIT_SUCCESS;
    }
    if (!ok) {
        return EXIT_FAILURE;
    }
    for (c = 0; c < 3; ++c) {
        n = profile.curves[c].table_length;
        if (n != 4096u || profile.curves[c].table == NULL) {
            ok = 0;
            break;
        }
        for (i = 0u; i < n; ++i) {
            /* Integer-rational oracle, independent of pow and parser math. */
            numerator = 4L * (long)i - (long)(n - 1u);
            expected = numerator <= 0 ? 0u :
                numerator >= (long)(n - 1u) ? 65535u :
                (unsigned int)((numerator * 65535L + (long)((n - 1u) / 2u)) /
                    (long)(n - 1u));
            if (profile.curves[c].table[i] != expected) {
                ok = 0;
                break;
            }
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static SIXELSTATUS
iccn_capture(sixel_frame_t *frame, void *context)
{
    float const *rgb;
    int *calls;
    int c;
    /* P3 red -> canonical linear sRGB, from primary chromaticities.
     * ICC fixed-point serialization needs a 3e-4 error allowance. */
    double const expected[3] = {
        1.2249401762805598, -0.0420569547096882, -0.0196375545903344
    };

    calls = (int *)context;
    ++*calls;
    if (frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->width != 1 || frame->height != 1 ||
        frame->colorspace != SIXEL_COLORSPACE_LINEAR) {
        return SIXEL_BAD_INPUT;
    }
    rgb = (float const *)sixel_frame_get_pixels(frame);
    for (c = 0; c < 3; ++c) {
        if (!isfinite(rgb[c]) || fabs(rgb[c] - expected[c]) > 3e-4) {
            return SIXEL_BAD_INPUT;
        }
    }
    return SIXEL_OK;
}

int
icc_numeric_loader_test(int segmented)
{
    unsigned char bytes[142u + sizeof(color_icc) + 56u];
    size_t size;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *loader;
    sixel_chunk_t *chunk;
    loader_probe_callback_state_t callback;
    SIXELSTATUS status;
    int engine;
    int calls;
    int result;

    allocator = NULL;
    loader = NULL;
    chunk = NULL;
    calls = 0;
    result = EXIT_FAILURE;
    engine = SIXEL_CMS_ENGINE_BUILTIN;
    callback = (loader_probe_callback_state_t){NULL, iccn_capture, &calls};
    memset(bytes, 0, sizeof(bytes));
    /* One opaque red BGR pixel followed by a standard V5 embedded profile. */
    memcpy(bytes, "BM", 2u);
    iccn_le32(bytes + 10, 138u);
    iccn_le32(bytes + 14, 124u);
    iccn_le32(bytes + 18, 1u);
    iccn_le32(bytes + 22, 1u);
    bytes[26] = 1u;
    bytes[28] = 24u;
    memcpy(bytes + 70, "DEBM", 4u);
    iccn_le32(bytes + 126, 128u);
    size = iccn_profile(bytes + 142, segmented, 1);
    iccn_le32(bytes + 130, (unsigned int)size);
    size += 142u;
    iccn_le32(bytes + 2, (unsigned int)size);
    bytes[140] = 255u;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status) || SIXEL_FAILED(create_loader_component_by_name(
            "builtin", allocator, (void **)&loader))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_loader_component_setopt(loader,
            SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine)) ||
        SIXEL_FAILED(sixel_chunk_create_from_memory(&chunk, bytes, size,
            NULL, allocator))) {
        goto end;
    }
    status = sixel_loader_component_load(loader, chunk,
                                           capture_frame_trampoline, &callback);
    if (!SIXEL_FAILED(status) || calls != 0) {
        goto end;
    }
    chunk->vtbl->unref(chunk);
    chunk = NULL;
    /* Retry a valid profile on the same loader, and observe extended RGB. */
    memcpy(bytes + 142, color_icc, sizeof(color_icc));
    size = 142u + sizeof(color_icc);
    iccn_le32(bytes + 2, (unsigned int)size);
    iccn_le32(bytes + 130, sizeof(color_icc));
    if (SIXEL_FAILED(sixel_chunk_create_from_memory(&chunk, bytes, size,
            NULL, allocator))) {
        goto end;
    }
    status = sixel_loader_component_load(loader, chunk,
                                           capture_frame_trampoline, &callback);
    if (SIXEL_SUCCEEDED(status) && calls == 1) {
        result = EXIT_SUCCESS;
    }
end:
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    if (loader != NULL) {
        sixel_loader_component_unref(loader);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return result;
}
