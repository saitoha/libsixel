/* SPDX-License-Identifier: MIT */
/* Finite fixed-point coefficients can still produce NaN/Inf in pow().
 * Profiles and a one-pixel BMP are constructed in memory, without a CMM
 * oracle or fixture I/O. Every caller reports one independent observation. */
#include <float.h>
#include <math.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "tests/_support/diagnostics/icc_numeric_common.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/mpet-storage-png.h"
#include "src/icc-parse.h"
#include "src/icc-apply.h"
#include "src/cms.h"
#include "src/loader-common.h"
#include "src/gamut.h"
#include "src/palette-snap-grid.h"

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

/* The two legacy tags map normalized XYZ to and from RGB identically.
 * Both floating-point slots share an on-disk tag. Its matrices would
 * return zero if evaluated, so calling one in place of a legacy tag is
 * observable with any nonzero input. */
#define ICCN_MPET_OFFSET 316u
#define ICCN_MPET_TEST_BYTES (ICCN_MPET_OFFSET + 16u + 257u * 8u + 92u)

static size_t
iccn_make_mpet_profile(unsigned char *bytes, unsigned int count)
{
    static char const tags[][5] = { "A2B0", "B2A0", "D2B0", "B2D0" };
    unsigned char *entry;
    unsigned char *mpet;
    unsigned char *matrix;
    unsigned int i;
    unsigned int j;
    unsigned int offset;
    unsigned int data_start;
    unsigned int length;

    data_start = 16u + count * 8u;
    length = data_start + (count == 4u ? 92u : 60u);
    memset(bytes, 0, ICCN_MPET_OFFSET + length);
    iccn_be32(bytes, ICCN_MPET_OFFSET + length);
    bytes[8] = 4u;
    memcpy(bytes + 12u, "mntrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 128u, 4u);

    for (i = 0u; i < 4u; ++i) {
        entry = bytes + 132u + i * 12u;
        memcpy(entry, tags[i], 4u);
        offset = i < 2u ? 180u + i * 68u : ICCN_MPET_OFFSET;
        iccn_be32(entry + 4u, offset);
        iccn_be32(entry + 8u, i < 2u ? 68u : length);
        if (i < 2u) {
            memcpy(bytes + offset, i == 0u ? "mAB " : "mBA ", 4u);
            bytes[offset + 8u] = bytes[offset + 9u] = 3u;
            iccn_be32(bytes + offset + 12u, 32u);
            for (j = 0u; j < 3u; ++j) {
                memcpy(bytes + offset + 32u + j * 12u, "curv", 4u);
            }
        }
    }

    mpet = bytes + ICCN_MPET_OFFSET;
    memcpy(mpet, "mpet", 4u);
    mpet[9] = mpet[11] = 3u;
    iccn_be32(mpet + 12u, count);
    matrix = mpet + data_start;
    memcpy(matrix, "matf", 4u);
    matrix[9] = count == 4u ? 2u : 3u;
    matrix[11] = 3u;
    if (count == 4u) {
        /* Physical order is 2->3 then 3->2. The position table has
         * 3->2 then 2->3, and repeats both positions verbatim. */
        matrix += 48u;
        memcpy(matrix, "matf", 4u);
        matrix[9] = 3u;
        matrix[11] = 2u;
    }
    for (i = 0u; i < count; ++i) {
        entry = mpet + 16u + i * 8u;
        offset = count == 4u && i % 2u == 0u ? 48u : 0u;
        iccn_be32(entry, data_start + offset);
        iccn_be32(entry + 4u, count != 4u ? 60u : offset ? 44u : 48u);
    }
    return ICCN_MPET_OFFSET + length;
}

int
icc_numeric_mpet_order_test(void)
{
    static uint32_t const offsets[] = {96u, 48u, 96u, 48u};
    static uint32_t const lengths[] = {44u, 48u, 44u, 48u};
    static uint16_t const channels[] = {3u, 2u, 3u, 2u, 3u};
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_icc_profile_t profile;
    sixel_icc_mpet_t const *mpet;
    sixel_icc_issue_t issue;
    size_t length;
    size_t direction;
    size_t i;
    int ok;

    length = iccn_make_mpet_profile(bytes, 4u);
    if (!sixel_icc_parse_profile_ex(bytes, length, &profile, &issue)) {
        return EXIT_FAILURE;
    }
    memset(bytes, 0, length);
    ok = issue == SIXEL_ICC_ISSUE_NONE;
    for (direction = 0u; direction < 2u; ++direction) {
        mpet = direction == 0u ? &profile.d2b_mpet[0] : &profile.b2d_mpet[0];
        if (mpet->element_count != 4u ||
            mpet->input_channels != 3u || mpet->output_channels != 3u ||
            mpet->elements == NULL || mpet->data == NULL) {
            ok = 0;
            break;
        }
        for (i = 0u; i < 4u; ++i) {
            ok &= mpet->elements[i].kind == SIXEL_ICC_MPE_MATRIX &&
                mpet->elements[i].offset == offsets[i] &&
                mpet->elements[i].length == lengths[i] &&
                mpet->elements[i].input_channels == channels[i] &&
                mpet->elements[i].output_channels == channels[i + 1u] &&
                memcmp(mpet->data + offsets[i], "matf", 4u) == 0;
        }
    }
    sixel_icc_profile_destroy(&profile);
    ok &= profile.d2b_mpet[0].element_count == 0u &&
        profile.d2b_mpet[0].data == NULL &&
        profile.b2d_mpet[0].element_count == 0u &&
        profile.b2d_mpet[0].elements == NULL;
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
iccn_mpet_rejected(unsigned char const *bytes, size_t length,
                    sixel_icc_issue_t expected)
{
    sixel_icc_profile_t profile;
    sixel_icc_issue_t issue;
    int ok;

    if (!sixel_icc_parse_profile_ex(bytes, length, &profile, &issue)) {
        return 0;
    }
    ok = issue == expected &&
        profile.d2b_mpet[0].element_count == 0u &&
        profile.d2b_mpet[0].elements == NULL &&
        profile.d2b_mpet[0].data == NULL &&
        profile.b2d_mpet[0].element_count == 0u;
    sixel_icc_profile_destroy(&profile);
    return ok;
}

int
icc_numeric_mpet_position_test(void)
{
    static unsigned int const fields[][2] = {
        {12u, 0u}, {12u, 8192u}, {16u, 40u}, {16u, 97u},
        {16u, 0xfffffffc}, {20u, 0xffffffff}, {20u, 11u}
    };
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    size_t i;
    size_t length;

    for (i = 0u; i < sizeof(fields) / sizeof(*fields); ++i) {
        length = iccn_make_mpet_profile(bytes, 4u);
        iccn_be32(bytes + ICCN_MPET_OFFSET + fields[i][0], fields[i][1]);
        if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)) {
            fprintf(stderr, "Accepted invalid mpet position case %zu\n", i);
            return EXIT_FAILURE;
        }
    }

    /* This 2x2 CLUT has a valid shape, but its range overlaps the matrix
     * that begins inside it. Its channel connection is intact. */
    length = iccn_make_mpet_profile(bytes, 4u);
    memcpy(bytes + ICCN_MPET_OFFSET + 48u, "clut", 4u);
    bytes[ICCN_MPET_OFFSET + 60u] = bytes[ICCN_MPET_OFFSET + 61u] = 2u;
    iccn_be32(bytes + ICCN_MPET_OFFSET + 28u, 76u);
    iccn_be32(bytes + ICCN_MPET_OFFSET + 44u, 76u);
    return iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_channels_test(void)
{
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    unsigned char *mpet;
    size_t length;

    length = iccn_make_mpet_profile(bytes, 4u);
    mpet = bytes + ICCN_MPET_OFFSET;
    mpet[9] = 2u;
    if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)) {
        return EXIT_FAILURE;
    }
    /* Each substituted matrix has a valid coefficient count. Only
     * its connection in the position table is invalid. */
    length = iccn_make_mpet_profile(bytes, 4u);
    iccn_be32(mpet + 16u, 48u);
    iccn_be32(mpet + 20u, 48u);
    if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)) {
        return EXIT_FAILURE;
    }
    length = iccn_make_mpet_profile(bytes, 4u);
    iccn_be32(mpet + 24u, 96u);
    iccn_be32(mpet + 28u, 44u);
    if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)) {
        return EXIT_FAILURE;
    }
    length = iccn_make_mpet_profile(bytes, 4u);
    iccn_be32(mpet + 12u, 3u);
    /* Rebase all positions to keep the physical table bounds valid. */
    memmove(mpet + 40u, mpet + 48u, 92u);
    iccn_be32(mpet + 16u, 88u);
    iccn_be32(mpet + 24u, 40u);
    iccn_be32(mpet + 32u, 88u);
    return iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_unknown_test(void)
{
    static double const rgb[] = {0.125, 0.25, 0.375};
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_icc_profile_t profile;
    sixel_icc_issue_t issue;
    double xyz[3];
    double output[3];
    size_t length;
    size_t i;
    int ok;

    length = iccn_make_mpet_profile(bytes, 4u);
    memcpy(bytes + ICCN_MPET_OFFSET + 48u, "zzzz", 4u);
    if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_UNSUPPORTED) ||
        !sixel_icc_parse_profile_ex(bytes, length, &profile, &issue)) {
        return EXIT_FAILURE;
    }
    ok = sixel_icc_apply_device_to_physical_xyz(xyz, rgb, 3u, &profile, 0u)
        && sixel_icc_apply_physical_xyz_to_device(
            output, 3u, xyz, &profile, 0u);
    if (ok) {
        for (i = 0u; i < 3u; ++i) {
            ok &= fabs(xyz[i] - rgb[i] * (65535.0 / 32768.0)) <= 1e-12
                && fabs(output[i] - rgb[i]) <= 1e-12;
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_no_path_test(void)
{
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_icc_profile_t profile;
    sixel_icc_issue_t issue;
    size_t length;

    length = iccn_make_mpet_profile(bytes, 4u);
    memset(bytes + 132u, 'x', 4u);
    memset(bytes + 144u, 'y', 4u);
    memcpy(bytes + ICCN_MPET_OFFSET + 48u, "zzzz", 4u);
    memset(&profile, 0, sizeof(profile));
    profile.kind = SIXEL_ICC_PROFILE_KIND_GRAY;
    if (sixel_icc_parse_profile_ex(bytes, length, &profile, &issue)) {
        sixel_icc_profile_destroy(&profile);
        return EXIT_FAILURE;
    }
    return issue == SIXEL_ICC_ISSUE_UNSUPPORTED &&
        profile.kind == SIXEL_ICC_PROFILE_KIND_GRAY
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_count_test(int over_limit)
{
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_icc_profile_t profile;
    sixel_icc_issue_t issue;
    size_t length;
    int ok;

    length = iccn_make_mpet_profile(bytes, over_limit ? 257u : 256u);
    if (!sixel_icc_parse_profile_ex(bytes, length, &profile, &issue)) {
        return over_limit && issue == SIXEL_ICC_ISSUE_RESOURCE_LIMIT
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    ok = !over_limit && issue == SIXEL_ICC_ISSUE_NONE &&
        profile.d2b_mpet[0].element_count == 256u &&
        profile.d2b_mpet[0].elements[255].offset == 2064u;
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_kinds_test(void)
{
    static char const signatures[][5] = { "cvst", "bACS", "clut", "eACS" };
    static unsigned int const sizes[] = {76u, 16u, 124u, 16u};
    static sixel_icc_mpe_kind_t const kinds[] = {
        SIXEL_ICC_MPE_CURVE_SET, SIXEL_ICC_MPE_BEGIN_ACS,
        SIXEL_ICC_MPE_CLUT, SIXEL_ICC_MPE_END_ACS
    };
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    unsigned char *mpet;
    unsigned char *element;
    sixel_icc_profile_t profile;
    unsigned int offset;
    size_t i;
    int ok;

    iccn_make_mpet_profile(bytes, 4u);
    mpet = bytes + ICCN_MPET_OFFSET;
    memset(mpet + 48u, 0, 232u);
    offset = 48u;
    for (i = 0u; i < 4u; ++i) {
        iccn_be32(mpet + 16u + i * 8u, offset);
        iccn_be32(mpet + 20u + i * 8u, sizes[i]);
        memcpy(mpet + offset, signatures[i], 4u);
        mpet[offset + 9u] = mpet[offset + 11u] = 3u;
        offset += sizes[i];
    }
    iccn_be32(bytes, ICCN_MPET_OFFSET + offset);
    iccn_be32(bytes + 164u, offset);
    iccn_be32(bytes + 176u, offset);

    /* Three curves share one single-segment identity formula. */
    element = mpet + 48u;
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(element + 12u + i * 8u, 36u);
        iccn_be32(element + 16u + i * 8u, 40u);
    }
    memcpy(element + 36u, "curf", 4u);
    element[45] = 1u;
    memcpy(element + 48u, "parf", 4u);
    iccn_be32(element + 60u, 0x3f800000u);
    iccn_be32(element + 64u, 0x3f800000u);
    element = mpet + 140u;
    element[12] = element[13] = element[14] = 2u;

    if (!sixel_icc_parse_profile(bytes, ICCN_MPET_OFFSET + offset, &profile)) {
        return EXIT_FAILURE;
    }
    ok = profile.d2b_mpet[0].element_count == 4u;
    if (ok) {
        for (i = 0u; i < 4u; ++i) {
            ok &= profile.d2b_mpet[0].elements[i].kind == kinds[i];
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

#define ICCN_MPE_OFFSET (ICCN_MPET_OFFSET + 32u)
#define ICCN_CURF_OFFSET (ICCN_MPE_OFFSET + 36u)

/* Both positions reference the same element, and both directions
 * reference the same tag. Legacy identity paths keep the profile usable. */
static unsigned char *
iccn_make_mpet_element(unsigned char *bytes, char const signature[4])
{
    unsigned char *tag;
    unsigned char *element;

    iccn_make_mpet_profile(bytes, 4u);
    tag = bytes + ICCN_MPET_OFFSET;
    memset(tag, 0, ICCN_MPET_TEST_BYTES - ICCN_MPET_OFFSET);
    memcpy(tag, "mpet", 4u);
    tag[9] = tag[11] = 3u;
    iccn_be32(tag + 12u, 2u);
    iccn_be32(tag + 16u, 32u);
    iccn_be32(tag + 24u, 32u);
    element = bytes + ICCN_MPE_OFFSET;
    memcpy(element, signature, 4u);
    element[9] = element[11] = 3u;
    return element;
}

static size_t
iccn_finish_mpet_element(unsigned char *bytes, unsigned int element_length)
{
    unsigned int tag_length;

    tag_length = 32u + element_length;
    iccn_be32(bytes + ICCN_MPET_OFFSET + 20u, element_length);
    iccn_be32(bytes + ICCN_MPET_OFFSET + 28u, element_length);
    iccn_be32(bytes + 164u, tag_length);
    iccn_be32(bytes + 176u, tag_length);
    iccn_be32(bytes, ICCN_MPET_OFFSET + tag_length);
    return ICCN_MPET_OFFSET + tag_length;
}

static void
iccn_mpe_write_formula(unsigned char *seg, unsigned int type,
                       unsigned int const *parameters)
{
    size_t count;
    size_t i;

    memcpy(seg, "parf", 4u);
    seg[9] = (unsigned char)type;
    count = type == 0u ? 4u : 5u;
    for (i = 0u; i < count; ++i) {
        iccn_be32(seg + 12u + i * 4u, parameters[i]);
    }
}

static size_t
iccn_make_mpet_formulas(unsigned char *bytes)
{
    static unsigned int const constant[] = {0x3f800000u, 0u, 0u, 0u};
    static unsigned int const parameters[][5] = {
        {0x40000000u, 0x40000000u, 0x3f000000u, 0xbfc00000u, 0u},
        {0x40000000u, 0x40000000u, 0x40400000u, 0x40800000u, 0xbe800000u},
        {0x3f000000u, 0x40800000u, 0x40000000u, 0x3f800000u, 0xc0000000u}
    };
    static unsigned int const offsets[] = {144u, 248u, 36u};
    unsigned char *cvst;
    unsigned char *curve;
    size_t i;

    cvst = iccn_make_mpet_element(bytes, "cvst");
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(cvst + 12u + i * 8u, offsets[i]);
        iccn_be32(cvst + 16u + i * 8u, i == 0u ? 104u : 108u);
        curve = cvst + offsets[i];
        memcpy(curve, "curf", 4u);
        curve[9] = 3u;
        iccn_be32(curve + 12u, 0xbf800000u);
        iccn_be32(curve + 16u, 0x40800000u);
        /* Restrict each nonlinear formula to the finite middle interval. */
        iccn_mpe_write_formula(curve + 20u, 0u, constant);
        iccn_mpe_write_formula(curve + 48u, (unsigned int)i, parameters[i]);
        iccn_mpe_write_formula(curve + (i == 0u ? 76u : 80u), 0u, constant);
    }
    return iccn_finish_mpet_element(bytes, 356u);
}

int
icc_mpet_curve_formulas_test(void)
{
    /* (2x + 0.5)^2 - 1.5, 2 log10(3x^2 + 4) - 0.25,
     * and 0.5 * 4^(2x + 1) - 2, respectively. */
    static double const input[] = {-0.75, 2.0, 0.25};
    static double const expected[] = {-0.5, 2.1582399653118496, 2.0};
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_icc_profile_t profile;
    double output[3];
    size_t length;
    size_t i;
    int ok;

    length = iccn_make_mpet_formulas(bytes);
    if (!sixel_icc_parse_profile(bytes, length, &profile)) {
        return EXIT_FAILURE;
    }
    memset(bytes, 0, length);
    ok = sixel_icc_apply_mpe_curves(output, input, 3u, &profile.d2b_mpet[0],
                                    0u);
    if (ok) {
        for (i = 0u; i < 3u; ++i) {
            ok &= fabs(output[i] - expected[i]) <= 1e-13;
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static size_t
iccn_make_mpet_samples(unsigned char *bytes)
{
    static unsigned int const first[] = {0x3f800000u, 0x3f800000u, 0u, 0u};
    static unsigned int const empty[] = {0x3f800000u, 0u, 0u, 0x42c80000u};
    static unsigned int const last[] = {0x3f800000u, 0x3f800000u, 0u,
                                        0x40800000u};
    unsigned char *cvst;
    unsigned char *curve;
    size_t i;

    cvst = iccn_make_mpet_element(bytes, "cvst");
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(cvst + 12u + i * 8u, 36u);
        iccn_be32(cvst + 16u + i * 8u, 148u);
    }
    curve = bytes + ICCN_CURF_OFFSET;
    memcpy(curve, "curf", 4u);
    curve[9] = 5u;
    iccn_be32(curve + 12u, 0xbf800000u);
    iccn_be32(curve + 16u, 0x3f800000u);
    iccn_be32(curve + 20u, 0x40000000u);
    iccn_be32(curve + 24u, 0x40000000u);
    iccn_mpe_write_formula(curve + 28u, 0u, first);
    memcpy(curve + 56u, "samf", 4u);
    iccn_be32(curve + 64u, 2u);
    iccn_be32(curve + 68u, 0x40000001u);
    iccn_be32(curve + 72u, 0x40400000u);
    memcpy(curve + 76u, "samf", 4u);
    iccn_be32(curve + 84u, 1u);
    iccn_be32(curve + 88u, 0xc0400000u);
    iccn_mpe_write_formula(curve + 92u, 0u, empty);
    iccn_mpe_write_formula(curve + 120u, 0u, last);
    return iccn_finish_mpet_element(bytes, 184u);
}

static int
iccn_frame_storage(sixel_frame_interface_t *iface, unsigned char const *bytes,
                   size_t length, size_t *storage)
{
    sixel_color_def_t *definition;
    SIXELSTATUS status;

    definition = sixel_cdef_icc(bytes, length);
    if (definition == NULL) {
        return 0;
    }
    status = iface->vtbl->set_rgb_definition(iface, definition);
    sixel_cdef_free(definition);
    return SIXEL_SUCCEEDED(status) &&
        SIXEL_SUCCEEDED(iface->vtbl->measure_storage(iface, storage));
}

int
icc_mpet_frame_storage_test(void)
{
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    float pixel[3] = {0};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    size_t length;
    size_t storage[3];
    size_t retained;
    int i;
    int result = EXIT_FAILURE;

    length = iccn_make_mpet_samples(bytes);
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixel, 1, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    for (i = 0; i < 3; ++i) {
        /* Keep the frame and ICC envelope the same size. First expose only
         * the legacy tags, then the shared MPE, then reject its final segment
         * after the sampled segments have already been allocated. */
        iccn_be32(bytes + 128u, i == 0 ? 2u : 4u);
        if (i == 2) {
            memcpy(bytes + ICCN_CURF_OFFSET + 120u, "bad!", 4u);
        }
        if (!iccn_frame_storage(iface, bytes, length, &storage[i])) {
            goto end;
        }
    }
    /* Both tags share one encoded copy. Two element positions share one
     * curve set, and all three channels share its five segments. */
    retained = length - ICCN_MPET_OFFSET + 2u * sizeof(sixel_icc_mpe_t)
        + 3u * sizeof(sixel_icc_mpe_curve_t)
        + 5u * sizeof(sixel_icc_mpe_segment_t) + 3u * sizeof(double);
    if (storage[1] == storage[0] + retained && storage[2] == storage[0]) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_frame_unref(frame);
    return result;
}

int
icc_segmented_frame_storage_test(void)
{
    unsigned char bytes[sizeof(color_icc)];
    unsigned char *curve;
    float pixel[3] = {0};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    size_t parametric;
    size_t segmented;
    int c;
    int result = EXIT_FAILURE;

    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixel, 1, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    memcpy(bytes, color_icc, sizeof(bytes));
    if (!iccn_frame_storage(iface, bytes, sizeof(bytes), &parametric)) {
        goto end;
    }
    /* Both representations retain one sampled table per channel. Only the
     * segmented parser needs temporary breakpoint and segment arrays. */
    for (c = 0; c < 3; ++c) {
        curve = bytes + iccn_read32(bytes + 136u + (3u + c) * 12u);
        memset(curve, 0, 32u);
        memcpy(curve, "segm", 4u);
        iccn_be32(curve + 8u, 2u);
        iccn_be32(curve + 12u, 0x8000u);
        iccn_be32(curve + 20u, 0x10000u);
        iccn_be32(curve + 28u, 0x10000u);
    }
    if (iccn_frame_storage(iface, bytes, sizeof(bytes), &segmented) &&
        segmented == parametric) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_frame_unref(frame);
    return result;
}

int
icc_mpet_curve_samples_test(void)
{
    /* Samples at 0 and 1 are 2 + 2^-22 and 3, with an implicit -1
     * at x=-1. The next interval interpolates from 3 to -3. */
    static double const pairs[][2] = {
        {-2.0, -2.0},
        {-1.0, -1.0},
        {-0.5, 0.5 + 0x1p-23},
        {0.0, 2.0 + 0x1p-22},
        {0.5, 2.5 + 0x1p-23},
        {1.0, 3.0},
        {1.5, 0.0},
        {2.0, -3.0},
        {0x1.0000000000001p1, 6.0},
        {3.0, 7.0}
    };
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_icc_profile_t profile;
    sixel_icc_mpet_t const *mpet;
    double input[3];
    double output[3];
    size_t length;
    size_t i;
    int ok;

    length = iccn_make_mpet_samples(bytes);
    if (!sixel_icc_parse_profile(bytes, length, &profile)) {
        return EXIT_FAILURE;
    }
    mpet = &profile.b2d_mpet[0];
    /* Check both the shared element and the shared nested curve. */
    ok = mpet->element_count == 2u && mpet->elements[0].curves != NULL &&
        mpet->elements[1].curves == NULL &&
        mpet->elements[0].curves[1].segments == NULL;
    for (i = 0u; ok && i < sizeof(pairs) / sizeof(*pairs); ++i) {
        input[0] = input[1] = input[2] = pairs[i][0];
        ok = sixel_icc_apply_mpe_curves(output, input, 3u, mpet, 1u) &&
            fabs(output[0] - pairs[i][1]) <= 1e-14 &&
            output[1] == output[0] && output[2] == output[0];
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_mpet_curve_structure_test(void)
{
    static unsigned int const fields[][2] = {
        {ICCN_MPE_OFFSET + 12u, 37u},
        {ICCN_MPE_OFFSET + 16u, 0xffffffffu},
        {ICCN_MPE_OFFSET + 24u, 144u},
        {ICCN_CURF_OFFSET + 4u, 1u},
        {ICCN_CURF_OFFSET + 8u, 0u},
        {ICCN_CURF_OFFSET + 12u, 0x7fc00000u},
        {ICCN_CURF_OFFSET + 16u, 0xc0000000u},
        {ICCN_CURF_OFFSET + 64u, 0u},
        {ICCN_CURF_OFFSET + 64u, 0xffffffffu},
        {ICCN_CURF_OFFSET + 68u, 0x7f800000u},
        {ICCN_CURF_OFFSET + 124u, 1u}
    };
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    size_t length;
    size_t i;
    unsigned char *segment;

    for (i = 0u; i < sizeof(fields) / sizeof(*fields); ++i) {
        length = iccn_make_mpet_samples(bytes);
        iccn_be32(bytes + fields[i][0], fields[i][1]);
        if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)) {
            fprintf(stderr, "Accepted invalid cvst case %zu\n", i);
            return EXIT_FAILURE;
        }
    }
    length = iccn_make_mpet_samples(bytes);
    segment = bytes + ICCN_CURF_OFFSET + 28u;
    memcpy(segment, "samf", 4u);
    iccn_be32(segment + 8u, 4u);
    if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)) {
        return EXIT_FAILURE;
    }
    length = iccn_make_mpet_samples(bytes);
    segment = bytes + ICCN_CURF_OFFSET + 120u;
    memcpy(segment, "samf", 4u);
    iccn_be32(segment + 8u, 4u);
    return iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_mpet_curve_unsupported_test(void)
{
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    size_t length;

    length = iccn_make_mpet_samples(bytes);
    bytes[ICCN_CURF_OFFSET + 129u] = 3u;
    if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_UNSUPPORTED)) {
        return EXIT_FAILURE;
    }
    length = iccn_make_mpet_samples(bytes);
    memcpy(bytes + ICCN_CURF_OFFSET + 120u, "zzzz", 4u);
    return iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_UNSUPPORTED)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_mpet_curve_evaluation_failure_test(void)
{
    static double const input[] = {-0.75, 2.0, 0.25};
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_icc_profile_t profile;
    sixel_icc_mpet_t const *mpet;
    double output[3];
    size_t length;
    int ok;

    length = iccn_make_mpet_formulas(bytes);
    /* Type 0 takes a square root of -1. All coefficients are finite. */
    iccn_be32(bytes + ICCN_MPE_OFFSET + 204u, 0x3f000000u);
    if (!sixel_icc_parse_profile(bytes, length, &profile)) {
        return EXIT_FAILURE;
    }
    mpet = &profile.d2b_mpet[0];
    ok = mpet->element_count == 2u &&
        !sixel_icc_apply_mpe_curves(output, input, 3u, mpet, 0u);
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_mpet_curve_log_range_test(void)
{
    static unsigned int const overflow[] = {
        0x447a0000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0u
    };
    static unsigned int const underflow[] = {
        0x447a0000u, 0x3f800000u, 0x3f800000u, 0u, 0u
    };
    static double const input[] = {-0.75, 3.0, 0.25};
    /* log10(3^1000 + 1) and 1000 * log10(1/4). Neither power fits
     * in double, but both logarithms are representable. */
    static double const expected[] = {-0.5, 477.12125471966244,
                                     -602.0599913279624};
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    unsigned char *curve;
    sixel_icc_profile_t profile;
    double output[3];
    size_t length;
    size_t i;
    int ok;

    length = iccn_make_mpet_formulas(bytes);
    curve = bytes + ICCN_MPE_OFFSET;
    iccn_mpe_write_formula(curve + 248u + 48u, 1u, overflow);
    curve += 36u;
    iccn_be32(curve + 12u, 0x3e000000u);
    iccn_be32(curve + 16u, 0x3f800000u);
    iccn_mpe_write_formula(curve + 48u, 1u, underflow);
    if (!sixel_icc_parse_profile(bytes, length, &profile)) {
        return EXIT_FAILURE;
    }
    ok = sixel_icc_apply_mpe_curves(output, input, 3u, &profile.d2b_mpet[0],
                                    0u);
    if (ok) {
        for (i = 0u; i < 3u; ++i) {
            ok &= fabs(output[i] - expected[i]) <= 1e-12;
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_mpet_curve_sample_endpoint_test(void)
{
    static double const input[] = {0.0, -0.5, 1.0};
    /* y = 2^100 * (1 - x) / (2^100 + 1). The denominator rounds to
     * 2^100 at double precision, giving the reference values below. */
    static double const expected[] = {1.0, 1.5, 0.0};
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    unsigned char *curve;
    sixel_icc_profile_t profile;
    double output[3];
    size_t length;
    size_t i;
    int ok;

    length = iccn_make_mpet_samples(bytes);
    curve = bytes + ICCN_CURF_OFFSET;
    /* Sampled interval (-2^100, 1], with y=2^100 supplied by the
     * preceding -x formula, then samples 2^99 and 0. */
    iccn_be32(curve + 12u, 0xf1800000u);
    iccn_be32(curve + 44u, 0xbf800000u);
    iccn_be32(curve + 68u, 0x71000000u);
    iccn_be32(curve + 72u, 0u);
    if (!sixel_icc_parse_profile(bytes, length, &profile)) {
        return EXIT_FAILURE;
    }
    ok = sixel_icc_apply_mpe_curves(output, input, 3u, &profile.d2b_mpet[0],
                                    0u);
    if (ok) {
        for (i = 0u; i < 3u; ++i) {
            ok &= fabs(output[i] - expected[i]) <= 1e-14;
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_slot3_status_test(void)
{
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_cms_profile_t *profile;
    sixel_cms_profile_result_t report;
    sixel_cms_engine_t previous;
    size_t length;
    int ok;

    length = iccn_make_mpet_profile(bytes, 4u);
    memcpy(bytes + 156u, "D2B3", 4u);
    memcpy(bytes + 168u, "B2D3", 4u);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    profile = sixel_cms_open_profile_from_mem_ex(bytes, length, &report);
    ok = profile != NULL && report.status == SIXEL_CMS_STATUS_OK &&
        report.actual_engine == SIXEL_CMS_ENGINE_BUILTIN &&
        report.skipped_path == SIXEL_CMS_STATUS_OK;
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void
iccn_mpe_write_float(unsigned char *bytes, float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    iccn_be32(bytes, bits);
}

int
icc_mpet_float_matrix_test(void)
{
    static float const matrix_3x2[] = {
        0.5f + 0x1p-20f, -0.25f, 1.5f, 0.0f, 2.0f, -1.0f,
        -0x1p-20f, 0.5f
    };
    static float const matrix_2x3[] = {
        2.0f, 1.0f, -0.5f, 0.25f, 0.0f, -2.0f,
        0.125f, -0x1p-20f, 0.0625f
    };
    static double const from_pcs[] = {-0.5, 1.5, 0.25};
    static double const to_pcs[] = {-30.0, 40.0};
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    unsigned char *tag;
    sixel_icc_profile_t profile;
    double forward[3];
    double reverse[2];
    size_t length;
    size_t i;
    int ok;

    length = iccn_make_mpet_profile(bytes, 4u);
    memcpy(bytes + 20u, "Lab ", 4u);
    tag = bytes + ICCN_MPET_OFFSET;
    for (i = 0u; i < sizeof(matrix_2x3) / sizeof(*matrix_2x3); ++i) {
        iccn_mpe_write_float(tag + 60u + i * 4u, matrix_2x3[i]);
    }
    for (i = 0u; i < sizeof(matrix_3x2) / sizeof(*matrix_3x2); ++i) {
        iccn_mpe_write_float(tag + 108u + i * 4u, matrix_3x2[i]);
    }
    if (!sixel_icc_parse_profile(bytes, length, &profile)) {
        return EXIT_FAILURE;
    }
    memset(bytes, 0, length);
    /* Reverse row 0 is -1/4 minus the coefficient and offset increments.
     * Forward rows give -20, 25 and -80 before their three offsets. Both
     * tested positions share the payload of an earlier matrix element. */
    ok = profile.pcs == SIXEL_ICC_PROFILE_PCS_LAB &&
        sixel_icc_apply_mpe_element(reverse, 2u, from_pcs, 3u,
                                     &profile.b2d_mpet[0], 2u) &&
        sixel_icc_apply_mpe_element(forward, 3u, to_pcs, 2u,
                                     &profile.d2b_mpet[0], 3u) &&
        reverse[0] == -0.25 - 3.0 * 0x1p-21 && reverse[1] == 3.25 &&
        forward[0] == -19.875 && forward[1] == 25.0 - 0x1p-20 &&
        forward[2] == -79.9375;
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static size_t
iccn_make_mpet_clut(unsigned char *bytes)
{
    unsigned char *element;
    unsigned char *sample;
    int x;
    int y;
    int z;

    element = iccn_make_mpet_element(bytes, "clut");
    element[12u] = 3u;
    element[13u] = 2u;
    element[14u] = 4u;
    sample = element + 28u;
    /* The closed-form functions are linear in each axis separately.
     * The xy term distinguishes multilinear interpolation. */
    for (x = 0; x < 3; ++x) {
        for (y = 0; y < 2; ++y) {
            for (z = 0; z < 4; ++z) {
                iccn_mpe_write_float(sample, -4.0f + x + 2 * y + z / 8.0f);
                iccn_mpe_write_float(sample + 4u,
                                     2.0f + x * y + z / 4.0f + 0x1p-19f);
                iccn_mpe_write_float(sample + 8u, (x - y + z) * 0x1p-24f);
                sample += 12u;
            }
        }
    }
    return iccn_finish_mpet_element(bytes, (unsigned int)(sample - element));
}

int
icc_mpet_float_clut_test(void)
{
    static double const input[][3] = {
        {0.75, 0.25, 0.5}, {-0.75, 1.75, 0.75}, {1.0, 0.0, 1.0}
    };
    /* Evaluate the three functions at lattice coordinates
     * (1.5,.25,1.5), (0,1,2.25), and (2,0,3), respectively. */
    static double const expected[][3] = {
        {-1.8125, 2.75 + 0x1p-19, 2.75 * 0x1p-24},
        {-1.71875, 2.5625 + 0x1p-19, 1.25 * 0x1p-24},
        {-1.625, 2.75 + 0x1p-19, 5.0 * 0x1p-24}
    };
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    sixel_icc_profile_t profile;
    double output[3];
    size_t length;
    size_t i;
    size_t channel;
    int ok;

    length = iccn_make_mpet_clut(bytes);
    if (!sixel_icc_parse_profile(bytes, length, &profile)) {
        return EXIT_FAILURE;
    }
    memset(bytes, 0, length);
    ok = 1;
    for (i = 0u; ok && i < sizeof(input) / sizeof(*input); ++i) {
        ok = sixel_icc_apply_mpe_element(output, 3u, input[i], 3u,
                                         &profile.d2b_mpet[0], 1u);
        for (channel = 0u; ok && channel < 3u; ++channel) {
            ok = fabs(output[channel] - expected[i][channel]) <= 1e-13;
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_mpet_numeric_payload_test(void)
{
    static struct {
        unsigned int offset;
        unsigned int value;
    } const invalid[] = {
        {12u, 0x01020400u}, /* One grid point on an active axis. */
        {12u, 0x03020401u}, /* Nonzero unused grid byte. */
        {12u, 0x04020400u}, /* Declared grid exceeds the sample count. */
        {28u, 0xff800000u}  /* Nonfinite sample. */
    };
    unsigned char bytes[ICCN_MPET_TEST_BYTES];
    size_t length;
    size_t i;

    for (i = 0u; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        length = iccn_make_mpet_clut(bytes);
        iccn_be32(bytes + ICCN_MPE_OFFSET + invalid[i].offset,
                  invalid[i].value);
        if (!iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)) {
            return EXIT_FAILURE;
        }
    }
    length = iccn_make_mpet_profile(bytes, 4u);
    iccn_be32(bytes + ICCN_MPET_OFFSET + 108u, 0x7fc00000u);
    return iccn_mpet_rejected(bytes, length, SIXEL_ICC_ISSUE_MALFORMED)
        ? EXIT_SUCCESS : EXIT_FAILURE;
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
    unsigned char bytes[sizeof(color_icc) + 36u];
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
    memset(payload, 0, sizeof(bytes) - sizeof(color_icc));
    memcpy(payload, malformed ? "mft2" : "mpet", 4u);
    if (!malformed) {
        /* A complete element table with an unknown element is unsupported. */
        payload[9] = payload[11] = 3u;
        iccn_be32(payload + 12u, 1u);
        iccn_be32(payload + 16u, 24u);
        iccn_be32(payload + 20u, 12u);
        memcpy(payload + 24u, "zzzz", 4u);
        payload[33] = payload[35] = 3u;
    }
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
    iccn_be32(entry + 8u, sizeof(bytes) - sizeof(color_icc));

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

static int
iccn_check_absolute_luts(unsigned char const *src_bytes, size_t src_length,
                         unsigned char const *dst_bytes, size_t dst_length,
                         double const expected[2][3],
                         char const *const intents[2])
{
    static float const input[] = {0.5f, 0.375f, 0.25f};
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_engine_t previous;
    int result;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(src_bytes, src_length);
    destination = sixel_cms_open_profile_from_mem(dst_bytes, dst_length);
    result = iccn_check_absolute_pair(source, destination, input, expected,
                                      intents);
    sixel_cms_close_profile(destination);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return result;
}

int
icc_numeric_absolute_lut_test(int fallback)
{
    static unsigned int const src_white[] = {49152u, 57344u, 65536u};
    static unsigned int const dst_white[] = {65536u, 49152u, 32768u};
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
    unsigned char slot;

    /* Saturation-only profiles cannot supply the colorimetric basis. */
    slot = fallback ? '2' : '1';
    iccn_absolute_lut_profile(src_bytes, src_white, slot);
    iccn_absolute_lut_profile(dst_bytes, dst_white, slot);
    return iccn_check_absolute_luts(src_bytes, sizeof(src_bytes),
        dst_bytes, sizeof(dst_bytes),
        fallback ? expected_fallback : expected,
        fallback ? fallback_intents : iccn_absolute_intents);
}

int
icc_numeric_absolute_equal_white_test(void)
{
    static unsigned int const white[] = {49152u, 57344u, 65536u};
    static double const expected[2][3] = {
        {0.5, 0.375, 0.25}, {0.5, 0.375, 0.25}
    };
    static char const *const intents[] = {"perceptual!", "absolute!"};
    unsigned char bytes[312];

    /* With equal media whites, the only LUT pair is an identity. */
    iccn_absolute_lut_profile(bytes, white, '0');
    return iccn_check_absolute_luts(bytes, sizeof(bytes), bytes, sizeof(bytes),
                                    expected, intents);
}

int
icc_numeric_absolute_after_missing_relative_test(void)
{
    static unsigned int const src_white[] = {49152u, 57344u, 65536u};
    static unsigned int const dst_white[] = {65536u, 49152u, 32768u};
    static double const expected[2][3] = {
        {0.375, 0.4375, 0.5}, {0.375, 0.4375, 0.5}
    };
    static char const *const intents[] = {"absolute!", "relative,absolute!"};
    unsigned char src_bytes[312];
    unsigned char dst_bytes[312];

    /* Neither endpoint has a slot 1. Absolute uses slot 0 on both sides
     * with the independently calculated media ratio (3/4, 7/6, 2). */
    iccn_absolute_lut_profile(src_bytes, src_white, '0');
    iccn_absolute_lut_profile(dst_bytes, dst_white, '0');
    return iccn_check_absolute_luts(src_bytes, sizeof(src_bytes),
                                    dst_bytes, sizeof(dst_bytes),
                                    expected, intents);
}

int
icc_numeric_absolute_unpaired_test(void)
{
    static unsigned int const src_white[] = {49152u, 57344u, 65536u};
    static unsigned int const dst_white[] = {65536u, 49152u, 32768u};
    static double const expected[2][3] = {
        {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}
    };
    static char const *const intents[] = {
        "saturation!", "absolute,saturation!"
    };
    unsigned char src_bytes[448];
    unsigned char dst_bytes[312];

    iccn_absolute_lut_profile(src_bytes, src_white, '1');
    iccn_absolute_lut_profile(dst_bytes, dst_white, '2');
    /* Add A2B2 with an all-zero CLUT beside the identity A2B1. The
     * destination has only B2A2, so a complete saturation pair gives zero. */
    memmove(src_bytes + 180u, src_bytes + 168u, 144u);
    iccn_be32(src_bytes, sizeof(src_bytes));
    iccn_be32(src_bytes + 128u, 4u);
    iccn_be32(src_bytes + 136u, 180u);
    iccn_be32(src_bytes + 148u, 180u);
    iccn_be32(src_bytes + 160u, 304u);
    memcpy(src_bytes + 168u, "A2B2", 4u);
    iccn_be32(src_bytes + 172u, 324u);
    iccn_be32(src_bytes + 176u, 124u);
    memcpy(src_bytes + 324u, src_bytes + 180u, 124u);
    memset(src_bytes + 388u, 0, 48u);
    return iccn_check_absolute_luts(src_bytes, sizeof(src_bytes),
                                    dst_bytes, sizeof(dst_bytes),
                                    expected, intents);
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
    if (sixel_frame_format(frame) != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->width != 1 || frame->height != 1 ||
        frame->format.coordinates != SIXEL_COLORSPACE_LINEAR) {
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

static void
iccn_profiled_bmp_header(unsigned char bytes[142], unsigned char red,
                          unsigned char green, unsigned char blue)
{
    /* One opaque BGR pixel followed by a V5 embedded profile. */
    memset(bytes, 0, 142u);
    memcpy(bytes, "BM", 2u);
    iccn_le32(bytes + 10u, 138u);
    iccn_le32(bytes + 14u, 124u);
    iccn_le32(bytes + 18u, 1u);
    iccn_le32(bytes + 22u, 1u);
    bytes[26] = 1u;
    bytes[28] = 24u;
    memcpy(bytes + 70u, "DEBM", 4u);
    iccn_le32(bytes + 126u, 128u);
    bytes[138] = blue;
    bytes[139] = green;
    bytes[140] = red;
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
    iccn_profiled_bmp_header(bytes, 255u, 0u, 0u);
    size = iccn_profile(bytes + 142, segmented, 1);
    iccn_le32(bytes + 130, (unsigned int)size);
    size += 142u;
    iccn_le32(bytes + 2, (unsigned int)size);
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

static void
iccn_mpet_position(unsigned char *tag, size_t index,
                    unsigned int offset, unsigned int length)
{
    iccn_be32(tag + 16u + index * 8u, offset);
    iccn_be32(tag + 20u + index * 8u, length);
}

static unsigned int
iccn_mpet_chain(unsigned char *tag)
{
    static unsigned int const curve[] = {
        0x3f800000u, 0x3f800000u, 0u, 0x3e000000u
    };
    static float const matrix[] = {
        0.0f, 1.0f, 0.0f, 0.25f, 0.0f, 1.0f, -0.25f, -0.125f
    };
    unsigned char *element;
    unsigned char *sample;
    size_t i;
    int u;
    int v;

    iccn_be32(tag + 12u, 4u);
    /* Physical order is matrix, placeholder, CLUT, curves. */
    iccn_mpet_position(tag, 0u, 184u, 76u);
    iccn_mpet_position(tag, 1u, 48u, 44u);
    iccn_mpet_position(tag, 2u, 92u, 16u);
    iccn_mpet_position(tag, 3u, 108u, 76u);
    element = tag + 48u;
    memcpy(element, "matf", 4u);
    element[9] = 3u;
    element[11] = 2u;
    for (i = 0u; i < 8u; ++i) {
        iccn_mpe_write_float(element + 12u + i * 4u, matrix[i]);
    }
    element = tag + 92u;
    memcpy(element, "bACS", 4u);
    element[9] = element[11] = 2u;
    element = tag + 108u;
    memcpy(element, "clut", 4u);
    element[9] = 2u;
    element[11] = 3u;
    element[12] = element[13] = 2u;
    sample = element + 28u;
    for (u = 0; u < 2; ++u) {
        for (v = 0; v < 2; ++v) {
            iccn_mpe_write_float(sample, u + v / 2.0f - 0.75f);
            iccn_mpe_write_float(sample + 4u, v + 1.0f);
            iccn_mpe_write_float(sample + 8u, u * v + 0x1p-20f);
            sample += 12u;
        }
    }
    element = tag + 184u;
    memcpy(element, "cvst", 4u);
    element[9] = element[11] = 3u;
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(element + 12u + i * 8u, 36u);
        iccn_be32(element + 16u + i * 8u, 40u);
    }
    memcpy(element + 36u, "curf", 4u);
    element[45] = 1u;
    iccn_mpe_write_formula(element + 48u, 0u, curve);
    return 260u;
}

static size_t
iccn_mpet_transform_profile(unsigned char bytes[516], int reverse,
                             char slot, int chain, unsigned int white,
                             int legacy)
{
    unsigned char *tag;
    unsigned char *element;
    unsigned int length;
    size_t i;

    memset(bytes, 0, 516u);
    bytes[8] = 4u;
    bytes[9] = 0x40u;
    memcpy(bytes + 12u, "prtrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 68u, 63190u);
    iccn_be32(bytes + 72u, 65536u);
    iccn_be32(bytes + 76u, 54061u);
    iccn_be32(bytes + 128u, legacy ? 3u : 2u);
    memcpy(bytes + 132u, reverse ? "B2D" : "D2B", 3u);
    bytes[135] = (unsigned char)slot;
    iccn_be32(bytes + 136u, 168u);
    memcpy(bytes + 144u, "wtpt", 4u);
    iccn_be32(bytes + 152u, 20u);
    tag = bytes + 168u;
    memcpy(tag, "mpet", 4u);
    tag[9] = tag[11] = 3u;
    if (chain) {
        length = iccn_mpet_chain(tag);
    } else {
        length = 84u;
        iccn_be32(tag + 12u, 1u);
        iccn_mpet_position(tag, 0u, 24u, 60u);
        element = tag + 24u;
        memcpy(element, "matf", 4u);
        element[9] = element[11] = 3u;
        for (i = 0u; i < 3u; ++i) {
            iccn_mpe_write_float(element + 12u + i * 16u, 1.0f);
        }
    }
    iccn_be32(bytes + 140u, length);
    length += 168u;
    iccn_be32(bytes + 148u, length);
    memcpy(bytes + length, "XYZ ", 4u);
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(bytes + length + 8u + i * 4u, white);
    }
    length += 20u;
    if (legacy) {
        /* A competing legacy path has identical identity B curves.
         * The independent expectation requires the standard path. */
        memcpy(bytes + 156u, reverse ? "B2A" : "A2B", 3u);
        bytes[159] = (unsigned char)slot;
        iccn_be32(bytes + 160u, length);
        iccn_be32(bytes + 164u, 68u);
        element = bytes + length;
        memcpy(element, reverse ? "mBA " : "mAB ", 4u);
        element[8] = element[9] = 3u;
        iccn_be32(element + 12u, 32u);
        for (i = 0u; i < 3u; ++i) {
            memcpy(element + 32u + i * 12u, "curv", 4u);
        }
        length += 68u;
    }
    iccn_be32(bytes, length);
    return length;
}

static int
iccn_check_mpet_transform(unsigned char const *src_bytes, size_t src_length,
                           unsigned char const *dst_bytes, size_t dst_length,
                           char const *intent, float const input[3],
                           double const expected[3])
{
    sixel_cms_profile_t *src;
    sixel_cms_profile_t *dst;
    sixel_cms_transform_t *transform;
    sixel_cms_engine_t previous;
    float output[3];
    size_t i;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    src = sixel_cms_open_profile_from_mem(src_bytes, src_length);
    dst = sixel_cms_open_profile_from_mem(dst_bytes, dst_length);
    transform = NULL;
    if (src != NULL && dst != NULL &&
        sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", intent) == 0) {
        transform = sixel_cms_create_transform(
            src, SIXEL_CMS_PIXELFORMAT_RGB_F32, dst,
            SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    }
    ok = transform != NULL &&
        sixel_cms_do_transform(transform, input, output, 1u);
    for (i = 0u; ok && i < 3u; ++i) {
        if (!(fabs(output[i] - expected[i]) < 0x1p-23)) {
            fprintf(stderr, "%s channel %zu: expected %.9g, got %.9g\n",
                    intent, i, expected[i], (double)output[i]);
            ok = 0;
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst);
    sixel_cms_close_profile(src);
    sixel_cms_set_engine(previous);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_mpet_chain_test(int reverse)
{
    /* Curves add 1/8. The matrix gives (u,v) = (3/8,9/32) forward
     * and (1/4,19/32) reverse. The CLUT encodes (u+v/2-3/4, v+1,
     * u*v+2^-20). The other profile is the identity. */
    static float const input[][3] = {
        {1.5f, 0.5f, -0.125f}, {-0.25f, 0.375f, 0.625f}
    };
    static double const expected[][3] = {
        {-0.234375, 1.28125, 0.10546875 + 0x1p-20},
        {-0.203125, 1.59375, 0.1484375 + 0x1p-20}
    };
    unsigned char src[516];
    unsigned char dst[516];
    size_t src_length;
    size_t dst_length;
    char slot;

    slot = reverse ? '2' : '1';
    src_length = iccn_mpet_transform_profile(src, 0, slot, !reverse, 49152u,
                                              !reverse);
    dst_length = iccn_mpet_transform_profile(dst, 1, slot, reverse, 32768u,
                                              reverse);
    return iccn_check_mpet_transform(src, src_length, dst, dst_length,
        reverse ? "saturation!" : "relative!", input[reverse],
        expected[reverse]);
}

int
icc_mpet_absolute_test(int relative_endpoint)
{
    static float const input[] = {0.25f, 0.375f, 0.5f};
    static double const expected[][3] = {
        {0.25, 0.375, 0.5},
        {0.25 * (0.75 / 0.9642), 0.375 * 0.75, 0.5 * (0.75 / 0.8249)},
        {0.25 * (0.9642 / 0.5), 0.375 / 0.5, 0.5 * (0.8249 / 0.5)}
    };
    unsigned char src[516];
    unsigned char dst[516];
    size_t src_length;
    size_t dst_length;

    src_length = iccn_mpet_transform_profile(src, 0,
        relative_endpoint == 1 ? '1' : '3', 0, 49152u, 0);
    dst_length = iccn_mpet_transform_profile(dst, 1,
        relative_endpoint == 2 ? '1' : '3', 0, 32768u, 0);
    return iccn_check_mpet_transform(src, src_length, dst, dst_length,
        "absolute!", input, expected[relative_endpoint]);
}

int
icc_mpet_float_pcs_lab_test(int reverse)
{
    static float const input[][3] = {
        {42.0f, -25.0f, 20.0f}, {-0x1p-10f, 0.0f, 0.0f}
    };
    static double const expected[][3] = {
        /* f(Y)=0.5, f(X)=0.45, and f(Z)=0.4. */
        {0.9642 * 0.45 * 0.45 * 0.45, 0.125, 0.8249 * 0.064},
        /* The linear branch has slope (29/6)^2/3 below zero.
         * Only a* is nonzero and reads X relative to D50. */
        {0.0, -500.0 * (841.0 / 108.0) * 0x1p-10 / 0.9642, 0.0}
    };
    unsigned char src[516];
    unsigned char dst[516];
    size_t src_length;
    size_t dst_length;

    src_length = iccn_mpet_transform_profile(src, 0, '1', 0, 49152u, 0);
    dst_length = iccn_mpet_transform_profile(dst, 1, '1', 0, 32768u, 0);
    memcpy((reverse ? dst : src) + 20u, "Lab ", 4u);
    return iccn_check_mpet_transform(src, src_length, dst, dst_length,
        "relative!", input[reverse], expected[reverse]);
}

int
icc_mpet_missing_intent_test(void)
{
    static unsigned char const input[] = {64u, 128u, 192u};
    unsigned char src_bytes[516];
    sixel_cms_profile_t *src;
    sixel_cms_profile_t *dst;
    sixel_cms_transform_t *transform;
    sixel_cms_engine_t previous;
    unsigned char output[3];
    size_t length;
    int ok;

    length = iccn_mpet_transform_profile(src_bytes, 0, '2', 0, 49152u, 0);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    src = sixel_cms_open_profile_from_mem(src_bytes, length);
    dst = sixel_cms_create_srgb_profile();
    transform = NULL;
    if (src != NULL && dst != NULL &&
        sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT",
                            "relative!") == 0) {
        transform = sixel_cms_create_transform(
            src, SIXEL_CMS_PIXELFORMAT_RGB_8, dst,
            SIXEL_CMS_PIXELFORMAT_RGB_8, SIXEL_CMS_TRANSFORM_DEFAULT);
    }
    ok = transform != NULL &&
        !sixel_cms_do_transform(transform, input, output, 1u);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst);
    sixel_cms_close_profile(src);
    sixel_cms_set_engine(previous);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
iccn_check_profile_header(unsigned char const *bytes, size_t length,
                            sixel_cms_status_t expected)
{
    sixel_cms_profile_t *profile;
    sixel_cms_profile_result_t report;
    sixel_cms_engine_t previous;
    sixel_cms_engine_t actual;
    int open;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    profile = sixel_cms_open_profile_from_mem_ex(bytes, length, &report);
    actual = profile != NULL ? SIXEL_CMS_ENGINE_BUILTIN : SIXEL_CMS_ENGINE_NONE;
    open = profile != NULL;
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    if (report.status != expected || open != (expected == SIXEL_CMS_STATUS_OK)
        || report.actual_engine != actual) {
        fprintf(stderr, "ICC header: expected status %d, got %d (open=%d)\n",
                (int)expected, (int)report.status, open);
        return 0;
    }
    return 1;
}

int
icc_numeric_profile_header_test(int specimen)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static struct {
        size_t offset;
        char const *field;
        sixel_cms_status_t status;
        int matrix;
    } const cases[] = {
        { 8u, "\x02\x40\0\0", SIXEL_CMS_STATUS_OK, 0 },
        { 8u, "\x05\x00\0\0", SIXEL_CMS_STATUS_UNSUPPORTED_PROFILE, 0 },
        { 8u, "\x04\xa0\0\0", SIXEL_CMS_STATUS_MALFORMED_PROFILE, 0 },
        { 36u, "bad!", SIXEL_CMS_STATUS_MALFORMED_PROFILE, 0 },
        { 12u, "link", SIXEL_CMS_STATUS_UNSUPPORTED_PROFILE, 0 },
        { 12u, "abst", SIXEL_CMS_STATUS_UNSUPPORTED_PROFILE, 0 },
        { 12u, "nmcl", SIXEL_CMS_STATUS_UNSUPPORTED_PROFILE, 0 },
        { 12u, "priv", SIXEL_CMS_STATUS_UNSUPPORTED_PROFILE, 0 },
        { 12u, "spac", SIXEL_CMS_STATUS_OK, 0 },
        { 12u, "spac", SIXEL_CMS_STATUS_NO_PATH, 1 },
        { 8u, "\x04\x30\0\x01", SIXEL_CMS_STATUS_MALFORMED_PROFILE, 0 }
    };
    unsigned char bytes[sizeof(color_icc) + 312u];
    size_t length;

    if (cases[specimen].matrix) {
        length = sizeof(color_icc);
        memcpy(bytes, color_icc, length);
    } else {
        length = 312u;
        iccn_absolute_lut_profile(bytes, white, '0');
    }
    memcpy(bytes + cases[specimen].offset, cases[specimen].field, 4u);
    return iccn_check_profile_header(bytes, length, cases[specimen].status)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_profile_missing_header_test(void)
{
    static size_t const offsets[] = {8u, 12u, 36u};
    unsigned char bytes[sizeof(color_icc)];
    unsigned int fields;
    size_t i;

    /* Check every combination of the three historically absent fields. */
    for (fields = 0u; fields < 8u; ++fields) {
        memcpy(bytes, color_icc, sizeof(bytes));
        for (i = 0u; i < 3u; ++i) {
            if ((fields & (1u << i)) != 0u) {
                memset(bytes + offsets[i], 0, 4u);
            }
        }
        if (!iccn_check_profile_header(bytes, sizeof(bytes),
                                        SIXEL_CMS_STATUS_OK)) {
            fprintf(stderr, "Absent ICC fields mask %u\n", fields);
            return EXIT_FAILURE;
        }
    }

    /* An absent signature and version do not hide an explicit class. */
    memcpy(bytes + 12u, "link", 4u);
    return iccn_check_profile_header(bytes, sizeof(bytes),
                                     SIXEL_CMS_STATUS_UNSUPPORTED_PROFILE)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_colorspace_lab_fallback_test(int legacy)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static double const xyz[] = {0.9642 / 8.0, 1.0 / 8.0, 0.8249 / 8.0};
    static double const expected[] = {0.42, 128.0 / 255.0, 128.0 / 255.0};
    unsigned char bytes[312];
    sixel_icc_profile_t profile;
    double result[3];
    size_t i;
    int applied;

    iccn_absolute_lut_profile(bytes, white, '0');
    memcpy(bytes + 12u, "spacLab Lab ", 12u);
    if (legacy) {
        bytes[8] = 2u;
        bytes[9] = 0x20u;
    }
    if (!sixel_icc_parse_profile(bytes, sizeof(bytes), &profile)) {
        return EXIT_FAILURE;
    }
    applied = sixel_icc_apply_physical_xyz_to_device(result, 3u, xyz,
                                                     &profile, 1u);
    sixel_icc_profile_destroy(&profile);
    if (applied != legacy) {
        return EXIT_FAILURE;
    }
    if (applied) {
        /* The cube root of 1/8 gives neutral Lab at L*=42. */
        for (i = 0u; i < 3u; ++i) {
            if (!(fabs(result[i] - expected[i]) < 1e-12)) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

int
icc_numeric_mpet_runtime_fallback_test(void)
{
    static float const input[] = {0.125f, 0.25f, 0.375f};
    static double const expected[] = {
        0.125 * (65535.0 / 32768.0), 0.25 * (65535.0 / 32768.0),
        0.375 * (65535.0 / 32768.0)
    };
    unsigned char src[516];
    unsigned char dst[516];
    size_t src_length;
    size_t dst_length;

    src_length = iccn_mpet_transform_profile(src, 0, '1', 0, 49152u, 1);
    dst_length = iccn_mpet_transform_profile(dst, 1, '1', 0, 49152u, 0);
    /* D2B1 parses with finite coefficients, but R*FLT_MAX+FLT_MAX
     * exceeds the element domain. A2B1 has three identity B curves. */
    iccn_be32(src + 168u + 24u + 12u, 0x7f7fffffu);
    iccn_be32(src + 168u + 24u + 48u, 0x7f7fffffu);
    return iccn_check_mpet_transform(src, src_length, dst, dst_length,
        "relative!", input, expected);
}

int
icc_numeric_mpet_clut_budget_test(void)
{
    enum {
        ICCN_CLUT_BYTES = 128u * 128u * 32u * 3u * 4u,
        ICCN_CLUT_TAG_OFFSET = 144u,
        ICCN_CLUT_ELEMENT_BYTES = 28u + ICCN_CLUT_BYTES,
        ICCN_CLUT_TAG_BYTES = 24u + ICCN_CLUT_ELEMENT_BYTES
    };
    unsigned char valid[516];
    unsigned char *bytes;
    unsigned char *mpet;
    unsigned char *clut;
    unsigned char untouched[sizeof(sixel_icc_profile_t)];
    sixel_icc_profile_t parsed;
    sixel_icc_issue_t issue;
    size_t length;
    size_t retry_length;
    int accepted;
    int ok;

    retry_length = iccn_mpet_transform_profile(valid, 0, '1', 1, 49152u, 0);
    length = ICCN_CLUT_TAG_OFFSET + ICCN_CLUT_TAG_BYTES;
    bytes = calloc(1u, length);
    if (bytes == NULL) {
        return EXIT_FAILURE;
    }
    /* The 6 MiB encoded tag fits. Its 12 MiB decoded doubles exceed
     * the cumulative 16 MiB accounting once the retained copy is charged. */
    memcpy(bytes, valid, 128u);
    iccn_be32(bytes, (unsigned int)length);
    iccn_be32(bytes + 128u, 1u);
    memcpy(bytes + 132u, "D2B1", 4u);
    iccn_be32(bytes + 136u, ICCN_CLUT_TAG_OFFSET);
    iccn_be32(bytes + 140u, ICCN_CLUT_TAG_BYTES);
    mpet = bytes + ICCN_CLUT_TAG_OFFSET;
    memcpy(mpet, "mpet", 4u);
    mpet[9] = mpet[11] = 3u;
    iccn_be32(mpet + 12u, 1u);
    iccn_mpet_position(mpet, 0u, 24u, ICCN_CLUT_ELEMENT_BYTES);
    clut = mpet + 24u;
    memcpy(clut, "clut", 4u);
    clut[9] = clut[11] = 3u;
    clut[12] = clut[13] = 128u;
    clut[14] = 32u;

    memset(&parsed, 0xa5, sizeof(parsed));
    memcpy(untouched, &parsed, sizeof(untouched));
    accepted = sixel_icc_parse_profile_ex(bytes, length, &parsed, &issue);
    free(bytes);
    if (accepted) {
        sixel_icc_profile_destroy(&parsed);
        return EXIT_FAILURE;
    }
    if (issue != SIXEL_ICC_ISSUE_RESOURCE_LIMIT ||
        memcmp(untouched, &parsed, sizeof(parsed)) != 0) {
        return EXIT_FAILURE;
    }
    if (!sixel_icc_parse_profile_ex(valid, retry_length, &parsed, &issue)) {
        return EXIT_FAILURE;
    }
    ok = issue == SIXEL_ICC_ISSUE_NONE &&
        parsed.d2b_mpet[1].element_count == 4u;
    sixel_icc_profile_destroy(&parsed);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct iccn_bmp_expectation {
    double const *rgb;
    unsigned int calls;
} iccn_bmp_expectation_t;

static SIXELSTATUS
iccn_capture_mpet_bmp(sixel_frame_t *frame, void *context)
{
    iccn_bmp_expectation_t *expected;
    float const *rgb;
    size_t i;

    expected = context;
    ++expected->calls;
    if (sixel_frame_format(frame) != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->width != 1 || frame->height != 1 ||
        frame->format.coordinates != SIXEL_COLORSPACE_LINEAR) {
        return SIXEL_BAD_INPUT;
    }
    rgb = (float const *)sixel_frame_get_pixels(frame);
    for (i = 0u; i < 3u; ++i) {
        if (!(fabs(rgb[i] - expected->rgb[i]) < 5e-6)) {
            fprintf(stderr, "BMP mpet channel %zu: %.9g expected %.9g\n",
                    i, (double)rgb[i], expected->rgb[i]);
            return SIXEL_BAD_INPUT;
        }
    }
    return SIXEL_OK;
}

static SIXELSTATUS
iccn_load_mpet_bmp(sixel_loader_component_t *loader,
                   sixel_allocator_t *allocator, unsigned char *bytes,
                   size_t length, loader_probe_callback_state_t *callback)
{
    sixel_chunk_t *chunk;
    SIXELSTATUS status;

    iccn_le32(bytes + 130u, (unsigned int)length);
    length += 142u;
    iccn_le32(bytes + 2u, (unsigned int)length);
    chunk = NULL;
    status = sixel_chunk_create_from_memory(&chunk, bytes, length, NULL,
                                             allocator);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_component_load(loader, chunk,
            capture_frame_trampoline, callback);
        chunk->vtbl->unref(chunk);
    }
    return status;
}

int
icc_numeric_mpet_bmp_test(int absolute, int reject)
{
    /* Input (128,64,32)/255 gives u=G-1/8, v=B+R/4+1/32, and
     * XYZ=(u+v/2-3/4, 1+v, u*v+2^-20). The second row scales by
     * (3/4)/D50 before applying the Bradford-adapted sRGB inverse.
     * The inverse is derived from sRGB chromaticities in Decimal. */
    static double const rgb[][3] = {
        {-3.604594238817091, 2.930750989048729, -0.278390991252295},
        {-2.748369973803181, 2.211414118783991, -0.201803524551932}
    };
    unsigned char bytes[142u + 516u];
    unsigned char *profile;
    size_t length;
    iccn_bmp_expectation_t expected;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *loader;
    loader_probe_callback_state_t callback;
    SIXELSTATUS status;
    int engine;
    int ok;

    allocator = NULL;
    loader = NULL;
    expected.rgb = rgb[absolute];
    expected.calls = 0u;
    callback = (loader_probe_callback_state_t){
        NULL, iccn_capture_mpet_bmp, &expected
    };
    engine = SIXEL_CMS_ENGINE_BUILTIN;
    ok = 0;
    profile = bytes + 142u;
    iccn_profiled_bmp_header(bytes, 128u, 64u, 32u);
    length = iccn_mpet_transform_profile(profile, 0, '1', 1, 49152u, 0);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status) || SIXEL_FAILED(create_loader_component_by_name(
            "builtin", allocator, (void **)&loader))) {
        goto done;
    }
    if (SIXEL_FAILED(sixel_loader_component_setopt(loader,
            SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine)) ||
        sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT",
                            absolute ? "absolute!" : "relative!") != 0) {
        goto done;
    }

    if (reject != 0) {
        if (reject == 1) {
            iccn_be32(profile + 168u + 108u + 28u, 0x7fc00000u);
        } else {
            memcpy(profile + 12u, "link", 4u);
        }
        status = iccn_load_mpet_bmp(loader, allocator, bytes, length,
                                     &callback);
        if (status != SIXEL_BAD_INPUT || expected.calls != 0u) {
            fprintf(stderr, "BMP rejected ICC: status %x, %u callbacks\n",
                    (unsigned int)status, expected.calls);
            goto done;
        }
        length = iccn_mpet_transform_profile(profile, 0, '1', 1, 49152u, 0);
    }

    status = iccn_load_mpet_bmp(loader, allocator, bytes, length, &callback);
    ok = SIXEL_SUCCEEDED(status) && expected.calls == 1u;

done:
    if (loader != NULL) {
        sixel_loader_component_unref(loader);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
iccn_transform_pixels(sixel_cms_profile_t const *source,
                      sixel_cms_pixel_format_t src_format,
                      sixel_cms_profile_t const *target,
                      sixel_cms_pixel_format_t dst_format, char const *intent,
                      void const *input, void *output, size_t count)
{
    sixel_cms_transform_t *transform;
    int ok;

    if (source == NULL || target == NULL ||
        sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", intent) != 0) {
        return 0;
    }
    transform = sixel_cms_create_transform(source, src_format, target,
        dst_format, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = transform != NULL &&
        sixel_cms_do_transform(transform, input, output, count);
    sixel_cms_delete_transform(transform);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    return ok;
}

static int
iccn_float_result(float const *actual, double const *expected, size_t count)
{
    size_t i;

    for (i = 0u; i < count; ++i) {
        if (!(fabs(actual[i] - expected[i]) < 2e-7)) {
            fprintf(stderr, "sample %zu: expected %.9g, got %.9g\n",
                    i, expected[i], (double)actual[i]);
            return 0;
        }
    }
    return 1;
}

static void
iccn_mpet_overflow_matrix(unsigned char *element)
{
    /* Both operands are finite. Positive test inputs exceed the float range. */
    iccn_be32(element + 12u, 0x7f7fffffu);
    iccn_be32(element + 48u, 0x7f7fffffu);
}

int
icc_numeric_mpet_legacy_color_test(void)
{
    static unsigned char const input[] = {64u, 64u, 64u};
    /* Identity mAB returns legacy XYZ=(64/255,64/255,64/255).
     * Bradford D50-to-D65, inverse sRGB, and the sRGB OETF give these
     * rounded bytes. A physical PCS scale of almost two changes them. */
    static unsigned char const expected[] = {139u, 135u, 152u};
    unsigned char bytes[516];
    unsigned char output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t length;
    int failure;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    target = sixel_cms_create_srgb_profile();
    ok = target != NULL;
    for (failure = 0; ok && failure <= 1; ++failure) {
        length = iccn_mpet_transform_profile(bytes, 0, '1', 0, 65536u, 1);
        if (failure) {
            iccn_mpet_overflow_matrix(bytes + 168u + 24u);
        } else {
            bytes[135] = '2';
        }
        source = sixel_cms_open_profile_from_mem(bytes, length);
        ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_RGB_8,
            target, SIXEL_CMS_PIXELFORMAT_RGB_8, "relative!", input,
            output, 1u) && memcmp(output, expected, sizeof(expected)) == 0;
        sixel_cms_close_profile(source);
    }
    sixel_cms_close_profile(target);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_fallback_from_absolute_test(void)
{
    static unsigned char const input[] = {64u, 64u, 64u};
    /* Saturation's identity A2B2 returns 64/255 in each XYZ channel.
     * Bradford adaptation and sRGB encoding give these rounded bytes. */
    static unsigned char const expected[] = {139u, 135u, 152u};
    unsigned char bytes[516];
    unsigned char output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t length;
    int ok;

    length = iccn_mpet_transform_profile(bytes, 0, '3', 0, 49152u, 1);
    bytes[159] = '2';
    iccn_mpet_overflow_matrix(bytes + 168u + 24u);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(bytes, length);
    target = sixel_cms_create_srgb_profile();
    ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_RGB_8,
        target, SIXEL_CMS_PIXELFORMAT_RGB_8, "absolute,saturation!", input,
        output, 1u) && memcmp(output, expected, sizeof(expected)) == 0;
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static size_t
iccn_duplicate_mpet_matrix(unsigned char bytes[516], size_t length,
                            unsigned char new_slot)
{
    /* A two-tag matrix fixture reserves space for a third table entry. */
    iccn_be32(bytes + 128u, 3u);
    memcpy(bytes + 156u, bytes + 132u, 12u);
    bytes[159] = new_slot;
    iccn_be32(bytes + 160u, (unsigned int)length);
    memcpy(bytes + length, bytes + 168u, 84u);
    length += 84u;
    iccn_be32(bytes, (unsigned int)length);
    return length;
}

int
icc_numeric_mpet_fallback_to_absolute_test(void)
{
    static double const expected[] = {0.25, 0.375, 0.5, 0.0, 0.125, 0.75};
    unsigned char src[516];
    unsigned char dst[516];
    float pixels[] = {0.25f, 0.375f, 0.5f, -1.0f, 0.125f, 0.75f};
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t src_length;
    size_t dst_length;
    int ok;

    src_length = iccn_mpet_transform_profile(src, 0, '1', 0, 49152u, 0);
    dst_length = iccn_mpet_transform_profile(dst, 1, '1', 0, 32768u, 0);
    src_length = iccn_duplicate_mpet_matrix(src, src_length, '3');
    dst_length = iccn_duplicate_mpet_matrix(dst, dst_length, '3');
    iccn_mpet_overflow_matrix(src + 168u + 24u);
    /* On pixel 1 D2B1 overflows, so D2B3/B2D3 preserve the input.
     * On pixel 2 the relative matrix evaluates -FLT_MAX+FLT_MAX to 0.
     * The in-place buffer also catches fallback using modified inputs. */
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(src, src_length);
    target = sixel_cms_open_profile_from_mem(dst, dst_length);
    ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32, "relative,absolute!", pixels,
        pixels, 2u) && iccn_float_result(pixels, expected, 6u);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_matrix_target_intent_test(void)
{
    static float const input[] = {0.25f, 0.25f, 0.25f};
    /* Invert the stored P3 colorant matrix and apply its inverse TRCs.
     * Absolute first multiplies each XYZ component by 0.75/D50. */
    static double const expected[][3] = {
        {0.541132973261, 0.530287072441, 0.589295745559},
        {0.474842029592, 0.457919475835, 0.566362444335}
    };
    unsigned char src[516];
    size_t length;

    length = iccn_mpet_transform_profile(src, 0, '1', 0, 49152u, 0);
    return iccn_check_mpet_transform(src, length, color_icc, sizeof(color_icc),
        "relative,absolute!", input, expected[0]) == EXIT_SUCCESS &&
        iccn_check_mpet_transform(src, length, color_icc, sizeof(color_icc),
            "absolute!", input, expected[1]) == EXIT_SUCCESS
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_raw_lab_target_test(void)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static float const input[] = {
        (float)(0.9642 * 0.45 * 0.45 * 0.45), 0.125f, (float)(0.8249 * 0.064)
    };
    /* The normalized XYZ cube roots are 0.45, 0.5, and 0.4. */
    static double const expected[] = {0.42, -25.0 / 128.0, 20.0 / 128.0};
    unsigned char src[516];
    unsigned char dst[312];
    float lab[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t length;
    int ok;

    length = iccn_mpet_transform_profile(src, 0, '1', 0, 65536u, 0);
    iccn_absolute_lut_profile(dst, white, '0');
    memcpy(dst + 12u, "mntrLab Lab ", 12u);
    memcpy(dst + 144u, "cprt", 4u);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(src, length);
    target = sixel_cms_open_profile_from_mem(dst, sizeof(dst));
    ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        target, SIXEL_CMS_PIXELFORMAT_LAB_F32, "relative!", input,
        lab, 1u) && iccn_float_result(lab, expected, 3u);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_colorspace_lab_slot_zero_test(int reverse)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static float const lab[] = {0.58f, 0.0f, 0.0f};
    static float const rgb[] = {0.125f, 0.125f, 0.125f};
    unsigned char bytes[312];
    float output[3];
    double expected[3];
    double scale;
    double f;
    sixel_cms_profile_t *profile;
    sixel_cms_profile_t *linear;
    sixel_cms_engine_t previous;
    int ok;

    iccn_absolute_lut_profile(bytes, white, '0');
    memcpy(bytes + 12u, "spacLab Lab ", 12u);
    /* Both directions use slot 0. Halving L* makes it observable that
     * the LUT ran instead of analytically passing Lab through. */
    bytes[282] = 0x80u;
    bytes[283] = 0u;
    scale = 32768.0 / 65535.0;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    profile = sixel_cms_open_profile_from_mem(bytes, sizeof(bytes));
    linear = sixel_cms_create_linear_srgb_profile();
    if (reverse) {
        expected[0] = 0.42 * scale;
        expected[1] = expected[2] = 0.0;
        ok = iccn_transform_pixels(linear, SIXEL_CMS_PIXELFORMAT_RGB_F32,
            profile, SIXEL_CMS_PIXELFORMAT_LAB_F32, "relative!", rgb,
            output, 1u);
    } else {
        f = (58.0 * scale + 16.0) / 116.0;
        expected[0] = expected[1] = expected[2] = f * f * f;
        ok = iccn_transform_pixels(profile, SIXEL_CMS_PIXELFORMAT_LAB_F32,
            linear, SIXEL_CMS_PIXELFORMAT_RGB_F32, "relative!", lab,
            output, 1u);
    }
    ok = ok && iccn_float_result(output, expected, 3u);
    sixel_cms_close_profile(linear);
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_high_dimensional_clut_test(void)
{
    enum { axes = 16, cells = 1u << axes, width = 12 + 16 * axes };
    static double const input[] = {0.375, 0.5, 0.625};
    /* The simplex basis for x0*x15 is min(x0,x15); an affine channel
     * retains its exact value even outside the unit interval. */
    static double const expected[] = {0.375, 1.625, -0.5};
    sixel_icc_profile_t profile;
    double output[3];
    unsigned char *bytes;
    unsigned char *tag;
    unsigned char *matrix;
    unsigned char *clut;
    unsigned char *sample;
    size_t tag_length;
    size_t length;
    size_t i;
    unsigned int x;
    unsigned int y;
    unsigned int z;
    int ok;

    tag_length = 32u + width + 28u + cells * 12u;
    length = 144u + tag_length;
    bytes = calloc(1u, length);
    if (bytes == NULL) {
        return EXIT_FAILURE;
    }
    iccn_be32(bytes, (unsigned int)length);
    bytes[8] = 4u;
    memcpy(bytes + 12u, "scnrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 128u, 1u);
    memcpy(bytes + 132u, "D2B1", 4u);
    iccn_be32(bytes + 136u, 144u);
    iccn_be32(bytes + 140u, (unsigned int)tag_length);
    tag = bytes + 144u;
    memcpy(tag, "mpet", 4u);
    tag[9] = tag[11] = 3u;
    iccn_be32(tag + 12u, 2u);
    iccn_mpet_position(tag, 0u, 32u, width);
    iccn_mpet_position(tag, 1u, 32u + width, 28u + cells * 12u);
    matrix = tag + 32u;
    memcpy(matrix, "matf", 4u);
    matrix[9] = 3u;
    matrix[11] = axes;
    for (i = 0u; i < axes; ++i) {
        iccn_mpe_write_float(matrix + 12u + 4u * (i * 3u + i % 3u), 1.0f);
    }
    iccn_mpe_write_float(matrix + 12u + 4u * ((axes - 1u) * 3u), 0.0f);
    iccn_mpe_write_float(matrix + 12u + 4u * ((axes - 1u) * 3u + 2u), 1.0f);
    clut = matrix + width;
    memcpy(clut, "clut", 4u);
    clut[9] = axes;
    clut[11] = 3u;
    memset(clut + 12u, 2, axes);
    for (i = 0u; i < cells; ++i) {
        x = (i & (1u << 15u)) != 0u;
        y = (i & (1u << 14u)) != 0u;
        z = (i & 1u) != 0u;
        sample = clut + 28u + 12u * i;
        iccn_mpe_write_float(sample, (float)(x * z));
        iccn_mpe_write_float(sample + 4u, 1.125f + y);
        iccn_mpe_write_float(sample + 8u, -0.25f + (float)x - (float)z);
    }
    ok = sixel_icc_parse_profile(bytes, length, &profile);
    free(bytes);
    if (!ok) {
        return EXIT_FAILURE;
    }
    ok = sixel_icc_apply_device_to_physical_xyz(output, input, 3u, &profile,
        1u) && memcmp(output, expected, sizeof(expected)) == 0;
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_legacy_slot_pair_test(void)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static float const input[] = {0.25f, 0.375f, 0.5f};
    static double const expected[] = {0.25, 0.375, 0.5};
    unsigned char src[516];
    unsigned char dst[312];
    size_t length;

    /* The established legacy route takes A2B1 on the source and B2A2
     * on the target. Neither side can use D2B0 with this intent order. */
    length = iccn_mpet_transform_profile(src, 0, '1', 0, 65536u, 1);
    src[135] = '0';
    iccn_absolute_lut_profile(dst, white, '2');
    return iccn_check_mpet_transform(src, length, dst, sizeof(dst),
        "relative,saturation!", input, expected);
}

int
icc_numeric_mpet_lab_neutral_test(int reverse)
{
    static float const input[2][3] = {
        {0.5f, 0x1p-60f, -0x1p-61f}, {0.5f, 0x1p-53f, -0x1p-54f}
    };
    static float const expected[2][3] = {
        {1.0f, 0.78125f, -0.5f}, {0.005f, 0x1p-60f, -0x1p-61f}
    };
    unsigned char src[516];
    unsigned char dst[516];
    unsigned char *matrix;
    float output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t src_length;
    size_t dst_length;
    size_t i;
    int ok;

    src_length = iccn_mpet_transform_profile(src, 0, '1', 0, 65536u, 0);
    dst_length = iccn_mpet_transform_profile(dst, 1, '1', 0, 65536u, 0);
    if (reverse) {
        /* The identity B2D matrix returns XYZ as L*, a*, b*. The facade
         * scales L* by 1/100 and the two chroma channels by 1/128. */
        memcpy(dst + 16u, "Lab ", 4u);
    } else {
        /* The facade input is (50, 2^-53, -2^-54) in CIELAB. The D2B
         * matrix scales the chroma by 2^53 and the lightness by 1/64. */
        memcpy(src + 16u, "Lab ", 4u);
        matrix = src + 168u + 24u + 12u;
        iccn_mpe_write_float(matrix, 0.0f);
        iccn_mpe_write_float(matrix + 4u, 0x1p53f);
        iccn_mpe_write_float(matrix + 12u, 0x1p-6f);
        iccn_mpe_write_float(matrix + 16u, 0.0f);
        iccn_mpe_write_float(matrix + 32u, 0x1p53f);
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(src, src_length);
    target = sixel_cms_open_profile_from_mem(dst, dst_length);
    ok = iccn_transform_pixels(source,
        reverse ? SIXEL_CMS_PIXELFORMAT_RGB_F32 : SIXEL_CMS_PIXELFORMAT_LAB_F32,
        target,
        reverse ? SIXEL_CMS_PIXELFORMAT_LAB_F32 : SIXEL_CMS_PIXELFORMAT_RGB_F32,
        "relative!", input[reverse], output, 1u);
    for (i = 0u; ok && i < 3u; ++i) {
        /* An absolute tolerance would accept lost chroma in the reverse
         * path. These expected floats are rounded once for storage. */
        if (output[i] != expected[reverse][i]) {
            fprintf(stderr, "channel %zu: expected %a, got %a\n",
                i, (double)expected[reverse][i], (double)output[i]);
            ok = 0;
        }
    }
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_reachable_legacy_pair_test(void)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static float const input[] = {0.25f, 0.375f, 0.5f};
    static double const expected[] = {0.25, 0.375, 0.5};
    unsigned char src[516];
    unsigned char dst[312];
    size_t length;
    int result;

    /* The configured D2B0 can run, but has no matching reverse route.
     * The legacy identity pair is A2B1 on input and B2A2 on output. */
    length = iccn_mpet_transform_profile(src, 0, '1', 0, 65536u, 1);
    src[135] = '0';
    iccn_absolute_lut_profile(dst, white, '2');
    result = iccn_check_mpet_transform(src, length, dst, sizeof(dst),
        "relative,saturation,perceptual!", input, expected);
    if (result != EXIT_SUCCESS) {
        return result;
    }

    /* Also check the same legacy pair after a numeric D2B2 failure. */
    src[135] = '2';
    iccn_mpet_overflow_matrix(src + 168u + 24u);
    return iccn_check_mpet_transform(src, length, dst, sizeof(dst),
        "relative,saturation!", input, expected);
}

int
icc_numeric_colorspace_legacy_rgb8_test(void)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static unsigned char const input[] = {64u, 64u, 64u};
    static char const *const intents[] = {"perceptual!", "relative!"};
    /* The identity mft2 yields 64/255 in each legacy XYZ component.
     * Bradford adaptation, inverse sRGB primaries and the sRGB transfer
     * function give these rounded bytes. */
    static unsigned char const expected[] = {139u, 135u, 152u};
    unsigned char bytes[312];
    unsigned char output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t i;
    int ok;

    iccn_absolute_lut_profile(bytes, white, '0');
    memcpy(bytes + 12u, "spac", 4u);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(bytes, sizeof(bytes));
    target = sixel_cms_create_srgb_profile();
    ok = source != NULL && target != NULL;
    for (i = 0u; ok && i < sizeof(intents) / sizeof(intents[0]); ++i) {
        ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_RGB_8,
            target, SIXEL_CMS_PIXELFORMAT_RGB_8, intents[i], input,
            output, 1u);
        if (ok && memcmp(output, expected, sizeof(expected)) != 0) {
            fprintf(stderr, "%s: expected RGB (139,135,152), got (%u,%u,%u)\n",
                intents[i], (unsigned int)output[0], (unsigned int)output[1],
                (unsigned int)output[2]);
            ok = 0;
        }
    }
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_definition_absolute_fallback_test(int reverse)
{
    static double const input[] = {0.25, 0.375, 0.5};
    static double const pcs_white[] = {0.9642, 1.0, 0.8249};
    unsigned char bytes[516];
    double output[3];
    double scale;
    double expected;
    size_t length;
    size_t i;
    sixel_color_def_t *definition;
    int ok;

    length = iccn_mpet_transform_profile(bytes, reverse, '1', 0, 49152u, 1);
    bytes[159] = '0';
    iccn_mpet_overflow_matrix(bytes + 168u + 24u);
    if (reverse) {
        /* Reserve a fourth directory entry and provide a usable A2B0
         * as well, so the same reverse fixture opens before CMS-07. */
        memmove(bytes + 180u, bytes + 168u, length - 168u);
        iccn_be32(bytes + 128u, 4u);
        iccn_be32(bytes + 136u, 180u);
        iccn_be32(bytes + 148u, 264u);
        iccn_be32(bytes + 160u, 284u);
        memcpy(bytes + 168u, "A2B0", 4u);
        iccn_be32(bytes + 172u, (unsigned int)length + 12u);
        iccn_be32(bytes + 176u, 68u);
        memcpy(bytes + length + 12u, bytes + 284u, 68u);
        memcpy(bytes + length + 12u, "mAB ", 4u);
        length += 80u;
        iccn_be32(bytes, (unsigned int)length);
    }
    definition = sixel_cdef_icc(bytes, length);
    ok = definition != NULL;
    if (ok && reverse) {
        ok = sixel_cdef_from_xyz(definition, input, output,
                                SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC);
    } else if (ok) {
        ok = sixel_cdef_to_xyz(definition, input, output,
                              SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC);
    }
    /* Slot zero has identity B curves. Combine its integer XYZ
     * encoding with the media-white ratio in the requested direction. */
    for (i = 0u; ok && i < 3u; ++i) {
        scale = (65535.0 / 32768.0) * (0.75 / pcs_white[i]);
        expected = reverse ? input[i] / scale : input[i] * scale;
        if (!(fabs(output[i] - expected) < 1e-12)) {
            fprintf(stderr, "channel %zu: expected %.12g, got %.12g\n",
                i, expected, output[i]);
            ok = 0;
        }
    }
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

size_t
icc_numeric_rgba_legacy_profile(unsigned char bytes[516])
{
    size_t length;

    /* Relative A2B1 supplies the color. Reachable D2B2 takes the facade
     * through ordered PCS and exercises its legacy compatibility call. */
    length = iccn_mpet_transform_profile(bytes, 0, '1', 0, 65536u, 1);
    bytes[135] = '2';
    return length;
}

int
icc_numeric_mpet_simplex_zero_weight_test(void)
{
    static double const input[] = {0.5, 0.5, 0.5, 0.5, 0.5};
    sixel_icc_mpe_t clut;
    sixel_icc_mpet_t mpet;
    double values[32];
    double output;
    size_t i;

    memset(&clut, 0, sizeof(clut));
    memset(&mpet, 0, sizeof(mpet));
    clut.kind = SIXEL_ICC_MPE_CLUT;
    clut.input_channels = 5u;
    clut.output_channels = 1u;
    memset(clut.grid_points, 2, clut.input_channels);
    clut.values = values;
    mpet.input_channels = clut.input_channels;
    mpet.output_channels = clut.output_channels;
    mpet.element_count = 1u;
    mpet.elements = &clut;

    /* Tied input fractions give weight 1/2 to each end of the cell's
     * diagonal and zero weight to every intermediate vertex. */
    values[0] = 0.0;
    for (i = 1u; i < 31u; ++i) {
        values[i] = FLT_MAX;
    }
    values[31] = 1.0;
    if (!sixel_icc_apply_mpe_element(&output, 1u, input, 5u, &mpet, 0u) ||
        output != 0.5) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int
icc_numeric_mpet_lab_pcs_neutral_test(int reverse)
{
    static float const input[2][3] = {
        {0.0f, 0x1p-60f, 0.0f}, {(float)(0.9642 * 0x1p-65), 0.0f, 0.0f}
    };
    unsigned char src[516];
    unsigned char dst[516];
    unsigned char *matrix;
    double expected[3];
    size_t src_length;
    size_t dst_length;

    src_length = iccn_mpet_transform_profile(src, 0, '1', 0, 65536u, 0);
    dst_length = iccn_mpet_transform_profile(dst, 1, '1', 0, 65536u, 0);
    matrix = dst + 168u + 24u + 12u;
    expected[1] = expected[2] = 0.0;
    if (reverse) {
        /* XYZ is near black, so a* = 500 * (841/108) * X / Xn.
         * Amplify just that chroma and discard Y and Z in the output. */
        memcpy(dst + 20u, "Lab ", 4u);
        iccn_mpe_write_float(matrix, 0.0f);
        iccn_mpe_write_float(matrix + 4u, 0x1p45f);
        iccn_mpe_write_float(matrix + 16u, 0.0f);
        iccn_mpe_write_float(matrix + 32u, 0.0f);
        expected[0] = ((double)input[1][0] / 0.9642) *
            (500.0 * (841.0 / 108.0)) * 0x1p45;
    } else {
        /* With L*=b*=0 the linear inverse gives
         * X = Xn * (108/841) * (a* / 500). Amplify X by 2^70. */
        memcpy(src + 20u, "Lab ", 4u);
        iccn_mpe_write_float(matrix, 0x1p70f);
        expected[0] = 0.9642 * (108.0 / 841.0) * 0x1p10 / 500.0;
    }
    return iccn_check_mpet_transform(src, src_length, dst, dst_length,
        "relative!", input[reverse], expected);
}

int
icc_numeric_colorspace_legacy_gray8_test(void)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static char const *const intents[] = {"perceptual!", "relative!"};
    static unsigned char const input[] = {64u};
    /* The legacy D50 ramp produces linear gray 64/255. The sRGB
     * transfer function rounds that to 137 in each channel. */
    static unsigned char const expected[] = {137u, 137u, 137u};
    unsigned char bytes[224];
    unsigned char output[3];
    unsigned char *lut;
    unsigned char *p;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t i;
    int ok;

    memset(bytes, 0, sizeof(bytes));
    iccn_be32(bytes, sizeof(bytes));
    bytes[8] = 4u;
    memcpy(bytes + 12u, "spacGRAYXYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(bytes + 68u + i * 4u, white[i]);
    }
    iccn_be32(bytes + 128u, 1u);
    memcpy(bytes + 132u, "A2B0", 4u);
    iccn_be32(bytes + 136u, 144u);
    iccn_be32(bytes + 140u, 80u);
    lut = bytes + 144u;
    memcpy(lut, "mft2", 4u);
    lut[8] = 1u;
    lut[9] = 3u;
    lut[10] = lut[49] = lut[51] = 2u;
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(lut + 12u + i * 16u, 65536u);
    }
    lut[54] = lut[55] = 255u;
    p = lut + 62u;
    for (i = 0u; i < 3u; ++i) {
        p[i * 2u] = (unsigned char)((white[i] - (i == 1u)) >> 8);
        p[i * 2u + 1u] = (unsigned char)(white[i] - (i == 1u));
    }
    p = lut + 68u;
    for (i = 0u; i < 3u; ++i) {
        iccn_reverse_sample(&p, 1, 0u);
        iccn_reverse_sample(&p, 1, 255u);
    }

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(bytes, sizeof(bytes));
    target = sixel_cms_create_srgb_profile();
    ok = source != NULL && target != NULL;
    for (i = 0u; ok && i < sizeof(intents) / sizeof(intents[0]); ++i) {
        ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_GRAY_8,
            target, SIXEL_CMS_PIXELFORMAT_RGB_8, intents[i], input,
            output, 1u);
        if (ok && memcmp(output, expected, sizeof(expected)) != 0) {
            fprintf(stderr, "%s: expected RGB (137,137,137), got (%u,%u,%u)\n",
                intents[i], (unsigned int)output[0], (unsigned int)output[1],
                (unsigned int)output[2]);
            ok = 0;
        }
    }
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void
iccn_identity_matrix_with_b2a0(unsigned char bytes[412])
{
    static char const channels[] = "rgb";
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    unsigned char tables[312];
    unsigned char *tag;
    unsigned char *xyz;
    size_t i;

    iccn_absolute_lut_profile(tables, white, '0');
    memset(bytes, 0, 412u);
    memcpy(bytes, tables, 128u);
    memcpy(bytes + 12u, "mntr", 4u);
    iccn_be32(bytes, 412u);
    iccn_be32(bytes + 128u, 7u);
    for (i = 0u; i < 3u; ++i) {
        tag = bytes + 132u + i * 12u;
        xyz = bytes + 216u + i * 20u;
        memcpy(tag, "rXYZ", 4u);
        tag[0] = (unsigned char)channels[i];
        iccn_be32(tag + 4u, (unsigned int)(216u + i * 20u));
        iccn_be32(tag + 8u, 20u);
        memcpy(xyz, "XYZ ", 4u);
        iccn_be32(xyz + 8u + i * 4u, 65536u);

        tag += 36u;
        memcpy(tag, "rTRC", 4u);
        tag[0] = (unsigned char)channels[i];
        iccn_be32(tag + 4u, 276u);
        iccn_be32(tag + 8u, 12u);
    }
    memcpy(bytes + 276u, "curv", 4u);
    memcpy(bytes + 204u, "B2A0", 4u);
    iccn_be32(bytes + 208u, 288u);
    iccn_be32(bytes + 212u, 124u);
    memcpy(bytes + 288u, tables + 168u, 124u);
    for (i = 352u; i < 400u; i += 2u) {
        bytes[i] = 0x20u;
        bytes[i + 1u] = 0u;
    }
}

int
icc_numeric_mpet_legacy_reverse_precedence_test(void)
{
    static float const input[] = {0.25f, 0.375f, 0.5f};
    /* The reverse LUT returns this constant at every corner. */
    static double const expected[] = {
        8192.0 / 65535.0, 8192.0 / 65535.0, 8192.0 / 65535.0
    };
    unsigned char src[516];
    unsigned char dst[412];
    size_t length;

    length = iccn_mpet_transform_profile(src, 0, '1', 0, 65536u, 1);
    src[135] = '2';
    iccn_identity_matrix_with_b2a0(dst);
    return iccn_check_mpet_transform(src, length, dst, sizeof(dst),
        "relative,perceptual,saturation!", input, expected);
}

/* Reserve space for more tag records and shift existing payload offsets. */
static unsigned char *
iccn_expand_tag_directory(unsigned char *bytes, unsigned char const *base,
                           size_t length, unsigned int added)
{
    size_t tags;
    size_t end;
    size_t gap;
    size_t i;
    unsigned char *offset;

    tags = iccn_read32(base + 128u);
    end = 132u + tags * 12u;
    gap = added * 12u;
    memcpy(bytes, base, end);
    memcpy(bytes + end + gap, base + end, length - end);
    for (i = 0u; i < tags; ++i) {
        offset = bytes + 136u + i * 12u;
        iccn_be32(offset, iccn_read32(offset) + (unsigned int)gap);
    }
    iccn_be32(bytes + 128u, (unsigned int)tags + added);
    return bytes + end;
}

int
icc_numeric_owned_mpet_matrix_fallback_test(int reverse)
{
    static double const input[] = {0.25, 0.375, 0.5};
    unsigned char bytes[sizeof(color_icc) + 12u + 84u];
    unsigned char mpet[516];
    unsigned char *tag;
    sixel_color_def_t *reference;
    sixel_color_def_t *definition;
    double expected[3];
    double actual[3];
    int ok;

    /* Add one relative MPE to the existing Display P3 matrix/TRC
     * fixture without changing its colorant or curve payloads. */
    tag = iccn_expand_tag_directory(bytes, color_icc, sizeof(color_icc), 1u);
    iccn_be32(bytes, sizeof(bytes));
    memcpy(tag, reverse ? "B2D1" : "D2B1", 4u);
    iccn_be32(tag + 4u, sizeof(color_icc) + 12u);
    iccn_be32(tag + 8u, 84u);
    iccn_mpet_transform_profile(mpet, reverse, '1', 0, 65536u, 0);
    iccn_mpet_overflow_matrix(mpet + 168u + 24u);
    memcpy(bytes + sizeof(color_icc) + 12u, mpet + 168u, 84u);

    reference = sixel_cdef_icc(color_icc, sizeof(color_icc));
    definition = sixel_cdef_icc(bytes, sizeof(bytes));
    ok = reference != NULL && definition != NULL;
    if (ok && reverse) {
        ok = sixel_cdef_from_xyz(reference, input, expected, 1) &&
             sixel_cdef_from_xyz(definition, input, actual, 1);
    } else if (ok) {
        ok = sixel_cdef_to_xyz(reference, input, expected, 1) &&
             sixel_cdef_to_xyz(definition, input, actual, 1);
    }
    /* Output profile admission also probes both relative directions. */
    ok = ok && memcmp(expected, actual, sizeof(actual)) == 0 &&
        sixel_cdef_output_ready(definition, 1, 1);
    sixel_cdef_free(definition);
    sixel_cdef_free(reference);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_gray_intent_trc_test(int perceptual)
{
    /* Linear kTRC, D50 white, and an independent forward LUT.
     * The LUT maps every component to zero; relative must use kTRC. */
    static double const relative[] = {0.5, 0.5, 0.5};
    static double const black[] = {0, 0, 0};
    unsigned char bytes[280] = {0};
    unsigned char *lut;
    sixel_cms_profile_t *source, *target;
    sixel_cms_transform_t *transform;
    sixel_cms_status_t status;
    sixel_cms_engine_t previous;
    float input = 0.5f, output[3];
    int result;

    iccn_be32(bytes, sizeof(bytes));
    bytes[8] = 4;
    memcpy(bytes + 12u, "mntrGRAYXYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 68u, 63190u);
    iccn_be32(bytes + 72u, 65536u);
    iccn_be32(bytes + 76u, 54061u);
    iccn_be32(bytes + 128u, 3u);
    memcpy(bytes + 132u, "wtpt", 4u);
    iccn_be32(bytes + 136u, 168u);
    iccn_be32(bytes + 140u, 20u);
    memcpy(bytes + 144u, "kTRC", 4u);
    iccn_be32(bytes + 148u, 188u);
    iccn_be32(bytes + 152u, 12u);
    memcpy(bytes + 156u, perceptual ? "A2B0" : "A2B2", 4u);
    iccn_be32(bytes + 160u, 200u);
    iccn_be32(bytes + 164u, 80u);
    memcpy(bytes + 168u, "XYZ ", 4u);
    memcpy(bytes + 176u, bytes + 68u, 12u);
    memcpy(bytes + 188u, "curv", 4u);
    lut = bytes + 200u;
    memcpy(lut, "mft2", 4u);
    lut[8] = 1u;
    lut[9] = 3u;
    lut[10] = lut[49] = lut[51] = 2u;
    iccn_be32(lut + 12u, 65536u);
    iccn_be32(lut + 28u, 65536u);
    iccn_be32(lut + 44u, 65536u);
    lut[54] = lut[55] = 255u;
    lut[70] = lut[71] = lut[74] = lut[75] = lut[78] = lut[79] = 255u;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(bytes, sizeof(bytes));
    target = sixel_cms_create_linear_srgb_profile();
    /* Confirm the LUT works in its own intent before classifying a failure
     * of the independent relative path. */
    result = 2;
    if (iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_GRAY_F32,
            target, SIXEL_CMS_PIXELFORMAT_RGB_F32,
            perceptual ? "perceptual!" : "saturation!",
            &input, output, 1u) && iccn_float_result(output, black, 3u) &&
            sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT",
                                "relative!") == 0) {
        transform = sixel_cms_create_transform(source,
            SIXEL_CMS_PIXELFORMAT_GRAY_F32, target,
            SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
        if (transform != NULL) {
            if (sixel_cms_do_transform_ex(transform, &input, output, 1u,
                                         &status)) {
                result = iccn_float_result(output, relative, 3u) ? 0 : 2;
            } else if (status == SIXEL_CMS_STATUS_EVALUATION_FAILED) {
                result = 1;
            }
            sixel_cms_delete_transform(transform);
        }
        (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    }
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return result;
}

int
icc_numeric_frame_saturation_matrix_test(void)
{
    /* Display P3 matrix/TRC to linear sRGB via D50. The added LUT is the
     * identity in encoded XYZ and yields a different result if selected. */
    static double const expected[] = {0.03648986, 0.11974113, 0.22687401};
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    float input[] = {64 / 255.f, 96 / 255.f, 128 / 255.f};
    unsigned char bytes[sizeof(color_icc) + 12u + 124u];
    unsigned char lut_profile[312];
    unsigned char *tag;
    double rgb[3], xyz[3];
    size_t i;
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *definition;
    sixel_frame_interface_t *iface;
    int ok;

    tag = iccn_expand_tag_directory(bytes, color_icc, sizeof(color_icc), 1u);
    iccn_be32(bytes, sizeof(bytes));
    memcpy(tag, "A2B2", 4u);
    iccn_be32(tag + 4u, sizeof(color_icc) + 12u);
    iccn_be32(tag + 8u, 124u);
    iccn_absolute_lut_profile(lut_profile, white, '2');
    memcpy(bytes + sizeof(color_icc) + 12u, lut_profile + 168u, 124u);
    definition = sixel_cdef_icc(bytes, sizeof(bytes));
    ok = definition != NULL && SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL))
        && SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, input, 1, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1));
    if (ok) {
        iface = sixel_frame_as_interface(frame);
        ok = SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface,
            definition)) &&
            SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) &&
            iccn_float_result((float *)(void *)sixel_frame_get_pixels(frame),
                              expected, 3u);
    }
    for (i = 0; i < 3u; ++i) {
        rgb[i] = input[i];
    }
    ok = ok && sixel_cdef_to_xyz(definition, rgb, xyz,
                                SIXEL_CMS_INTENT_SATURATION);
    for (i = 0; ok && i < 3u; ++i) {
        ok = fabs(xyz[i] - rgb[i] * (65535.0 / 32768.0)) < 1e-6;
    }
    sixel_cdef_free(definition);
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_frame_storage_fallback_test(void)
{
    static double const expected[] = {
        0, 0, 0, 5.21434165, -0.98270027, 0.73229661, 0, 0, 0
    };
    static unsigned char const clipped[] = {255, 0, 255};
    float input[] = {0, .25f, .25f, 1, .25f, .25f, 0, .25f, .25f};
    unsigned char bytes[516];
    unsigned char *matrix;
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *def;
    sixel_frame_interface_t *iface;
    size_t length;
    int ok;

    /* D2B1 maps red to X=R*FLT_MAX. Only the middle pixel needs A2B1
     * to obtain storable linear sRGB. The first and last remain black
     * through D2B1 even with nonzero green and blue input. For the middle
     * pixel the oracle is the inverse Bradford-adapted sRGB matrix times
     * (1, .25, .25) * (65535/32768), the identity mft2 XYZ encoding. */
    length = iccn_mpet_transform_profile(bytes, 0, '1', 0, 65536u, 1);
    matrix = bytes + 168u + 24u;
    memset(matrix + 12u, 0, 48u);
    iccn_mpe_write_float(matrix + 12u, FLT_MAX);
    def = sixel_cdef_icc(bytes, length);
    ok = def != NULL && SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, input, 3, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1));
    if (!ok) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    ok = SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface, def)) &&
        SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) &&
        iccn_float_result((float *)(void *)sixel_frame_get_pixels(frame),
                          expected, 9u);
    /* The same finite PCS is valid for clipped RGB888. Its magenta
     * result distinguishes D2B1 from the green/cyan legacy alternative. */
    if (ok) {
        ok = SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, input + 3, 1, 1,
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) &&
            SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface, def)) &&
            SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
                SIXEL_PIXELFORMAT_RGB888)) &&
            memcmp(sixel_frame_get_pixels(frame), clipped, 3u) == 0;
    }
end:
    sixel_cdef_free(def);
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_float_storage_fallback_test(void)
{
    static float const input[] = {0.25f, 0.375f, 0.5f};
    /* Identity A2B1 encodes XYZ as input * (65535/32768). Applying the
     * inverse Bradford-adapted linear sRGB matrix gives these values. */
    static double const expected[] = {-0.13650551, 0.98114175, 1.26997089};
    unsigned char bytes[516];
    unsigned char *matrix;
    float output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t length;
    int ok;

    length = iccn_mpet_transform_profile(bytes, 0, '1', 0, 65536u, 1);
    matrix = bytes + 168u + 24u;
    memset(matrix + 12u, 0, 48u);
    /* This XYZ fits in the MPE, but its linear sRGB red exceeds FLT_MAX. */
    iccn_mpe_write_float(matrix + 48u, FLT_MAX);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(bytes, length);
    target = sixel_cms_create_linear_srgb_profile();
    ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32, "relative!", input, output,
        1u) && iccn_float_result(output, expected, 3u);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_simplex_cancellation_test(int amplify)
{
    static double const input[] = {0.25, 0.5, 0.75};
    static double const coordinates[] = {0.0, 0.0, 0.25, 0.5, 0.75};
    double expand[20] = {
        0.0, 0.0, 0.0,
        0.0, 0.0, 0.0,
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 0.0, 0.0
    };
    double scale[] = {0x1p60, 0.0, 0.0, 0.0, 0.0, 0.0};
    double values[32] = {1.0, 0x1p-60, 0.0, -1.0};
    sixel_icc_profile_t profile;
    sixel_icc_mpe_t elements[3];
    sixel_icc_mpet_t *mpet;
    double output[3];
    int ok;

    /* The simplex visits vertices 0, 1, 3, and 7, each at weight 1/4.
     * Cancellation of the unit samples leaves exactly 2^-62. */
    memset(&profile, 0, sizeof(profile));
    memset(elements, 0, sizeof(elements));
    profile.kind = SIXEL_ICC_PROFILE_KIND_RGB;
    profile.pcs = SIXEL_ICC_PROFILE_PCS_XYZ;
    mpet = &profile.d2b_mpet[1];
    mpet->input_channels = 3u;
    mpet->output_channels = 3u;
    mpet->element_count = 3u;
    mpet->elements = elements;

    elements[0].kind = SIXEL_ICC_MPE_MATRIX;
    elements[0].input_channels = 3u;
    elements[0].output_channels = 5u;
    elements[0].values = expand;
    elements[1].kind = SIXEL_ICC_MPE_CLUT;
    elements[1].input_channels = 5u;
    elements[1].output_channels = 1u;
    elements[1].payload_element = 1u;
    memset(elements[1].grid_points, 2, 5u);
    elements[1].values = values;
    elements[2].kind = SIXEL_ICC_MPE_MATRIX;
    elements[2].input_channels = 1u;
    elements[2].output_channels = 3u;
    elements[2].payload_element = 2u;
    elements[2].values = scale;

    if (amplify) {
        ok = sixel_icc_apply_device_to_physical_xyz(output, input, 3u,
            &profile, 1u) && output[0] == 0.25 &&
            output[1] == 0.0 && output[2] == 0.0;
    } else {
        ok = sixel_icc_apply_mpe_element(output, 1u, coordinates, 5u,
            mpet, 1u) && output[0] == 0x1p-62;
    }
    if (!ok) {
        fprintf(stderr, "simplex cancellation with amplification=%d\n",
                amplify);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

size_t
icc_numeric_gray_legacy_profile(unsigned char bytes[260])
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    unsigned char *tag;
    size_t i;

    memset(bytes, 0, 260u);
    iccn_be32(bytes, 260u);
    bytes[8] = 4u;
    memcpy(bytes + 12u, "mntrGRAYXYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    memcpy(bytes + 132u, "wtpt", 4u);
    iccn_be32(bytes + 128u, 3u);
    iccn_be32(bytes + 136u, 168u);
    iccn_be32(bytes + 140u, 20u);
    memcpy(bytes + 144u, "kTRC", 4u);
    iccn_be32(bytes + 148u, 188u);
    iccn_be32(bytes + 152u, 12u);
    /* The lower-priority saturation MPE enables ordered PCS, while
     * relative uses the identity gray TRC and the legacy byte path. */
    memcpy(bytes + 156u, "D2B2", 4u);
    iccn_be32(bytes + 160u, 200u);
    iccn_be32(bytes + 164u, 60u);
    memcpy(bytes + 168u, "XYZ ", 4u);
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(bytes + 68u + i * 4u, white[i]);
        iccn_be32(bytes + 176u + i * 4u, white[i]);
    }
    memcpy(bytes + 188u, "curv", 4u);
    tag = bytes + 200u;
    memcpy(tag, "mpet", 4u);
    tag[9] = 1u;
    tag[11] = 3u;
    iccn_be32(tag + 12u, 1u);
    iccn_mpet_position(tag, 0u, 24u, 36u);
    memcpy(tag + 24u, "matf", 4u);
    tag[33] = 1u;
    tag[35] = 3u;
    for (i = 0u; i < 3u; ++i) {
        iccn_mpe_write_float(tag + 36u + i * 4u, 1.0f);
    }
    return 260u;
}

static void
iccn_identity_a2b(unsigned char *tag)
{
    size_t i;

    memset(tag, 0, 68u);
    memcpy(tag, "mAB ", 4u);
    tag[8] = tag[9] = 3u;
    iccn_be32(tag + 12u, 32u);
    for (i = 0u; i < 3u; ++i) {
        memcpy(tag + 32u + 12u * i, "curv", 4u);
    }
}

int
icc_numeric_colorspace_legacy_float_pair_test(int byte_input)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static unsigned char const rgb8[] = {64u, 96u, 128u};
    static float const rgb_float[] = {0.25f, 0.375f, 0.5f};
    static double const expected[] = {
        8192.0 / 65535.0, 8192.0 / 65535.0, 8192.0 / 65535.0
    };
    unsigned char tables[312];
    unsigned char src[244];
    unsigned char dst[380];
    float output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t i;
    int ok;

    iccn_absolute_lut_profile(tables, white, '0');
    memcpy(tables + 12u, "spac", 4u);
    memcpy(src, tables, 156u);
    iccn_be32(src, sizeof(src));
    iccn_be32(src + 128u, 2u);
    src[135] = '1';
    iccn_be32(src + 136u, 156u);
    iccn_be32(src + 140u, 68u);
    memcpy(src + 144u, tables + 156u, 12u);
    iccn_be32(src + 148u, 224u);
    iccn_identity_a2b(src + 156u);
    memcpy(src + 224u, tables + 292u, 20u);

    memcpy(dst, tables, 168u);
    iccn_be32(dst, sizeof(dst));
    iccn_be32(dst + 140u, 68u);
    dst[147] = '2';
    iccn_be32(dst + 148u, 236u);
    iccn_be32(dst + 160u, 360u);
    iccn_identity_a2b(dst + 168u);
    memcpy(dst + 236u, tables + 168u, 144u);
    /* The forward LUTs are identity. The only reverse LUT returns the
     * same value at every vertex, independently of source quantization. */
    for (i = 300u; i < 348u; i += 2u) {
        dst[i] = 0x20u;
        dst[i + 1u] = 0u;
    }

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(src, sizeof(src));
    target = sixel_cms_open_profile_from_mem(dst, sizeof(dst));
    ok = iccn_transform_pixels(source, byte_input ? SIXEL_CMS_PIXELFORMAT_RGB_8
        : SIXEL_CMS_PIXELFORMAT_RGB_F32, target, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        "relative,saturation!", byte_input ? (void const *)rgb8 : rgb_float,
        output, 1u) && iccn_float_result(output, expected, 3u);
    if (!ok) {
        fprintf(stderr, "mixed ColorSpace LUTs with byte_input=%d failed\n",
                byte_input);
    }
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_legacy_mixed_precedence_test(int byte_input,
                                                int saturation_first)
{
    static unsigned int const white[] = {63190u, 65536u, 54061u};
    static unsigned char const rgb8[] = {64u, 96u, 128u};
    static float const rgb_float[] = {0.25f, 0.375f, 0.5f};
    unsigned char tables[312];
    unsigned char src[380];
    unsigned char dst[380];
    unsigned char with_mpet[476];
    unsigned char mpet[516];
    unsigned char *tag;
    unsigned char *matrix;
    unsigned char output8[3];
    float output_float[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    sixel_cms_pixel_format_t format;
    void const *input;
    void *output;
    size_t pixel_size;
    int added;
    int ok;

    /* Both profiles have a perceptual LUT. Only the source has the first
     * requested slot; pairing it with destination B2A0 gives identity.
     * BPC must leave this pairing intact when relative is not requested. */
    iccn_absolute_lut_profile(tables, white, '0');
    memcpy(src, tables, sizeof(tables));
    memcpy(dst, tables, sizeof(tables));
    iccn_be32(src, sizeof(src));
    iccn_be32(dst, sizeof(dst));
    memcpy(src + 144u, saturation_first ? "A2B2" : "A2B1", 4u);
    iccn_be32(src + 148u, sizeof(tables));
    iccn_be32(src + 152u, 68u);
    iccn_identity_a2b(src + sizeof(tables));
    memset(src + 232u, 0, 48u);
    iccn_be32(dst + 136u, sizeof(tables));
    iccn_be32(dst + 140u, 68u);
    iccn_identity_a2b(dst + sizeof(tables));

    /* Add a distinct saturation MPE that has no matching reverse path. */
    tag = iccn_expand_tag_directory(with_mpet, src, sizeof(src), 1u);
    iccn_be32(with_mpet, sizeof(with_mpet));
    memcpy(tag, "D2B2", 4u);
    iccn_be32(tag + 4u, sizeof(src) + 12u);
    iccn_be32(tag + 8u, 84u);
    iccn_mpet_transform_profile(mpet, 0, '2', 0, 65536u, 0);
    matrix = mpet + 192u;
    memset(matrix + 12u, 0, 36u);
    iccn_mpe_write_float(matrix + 48u, 0.875f);
    iccn_mpe_write_float(matrix + 52u, 0.875f);
    iccn_mpe_write_float(matrix + 56u, 0.875f);
    memcpy(with_mpet + sizeof(src) + 12u, mpet + 168u, 84u);

    format = byte_input ? SIXEL_CMS_PIXELFORMAT_RGB_8
                        : SIXEL_CMS_PIXELFORMAT_RGB_F32;
    input = byte_input ? (void const *)rgb8 : rgb_float;
    output = byte_input ? (void *)output8 : output_float;
    pixel_size = byte_input ? sizeof(rgb8) : sizeof(rgb_float);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    target = sixel_cms_open_profile_from_mem(dst, sizeof(dst));
    ok = target != NULL;
    for (added = 0; ok && added <= 1; ++added) {
        source = added
            ? sixel_cms_open_profile_from_mem(with_mpet, sizeof(with_mpet))
            : sixel_cms_open_profile_from_mem(src, sizeof(src));
        ok = iccn_transform_pixels(source, format, target, format,
            saturation_first == 2 ? "saturation,perceptual,relative!"
            : saturation_first ? "saturation,perceptual!"
                        : "relative,perceptual,saturation!",
            input, output, 1u) &&
            memcmp(input, output, pixel_size) == 0;
        sixel_cms_close_profile(source);
        if (!ok) {
            fprintf(stderr, "legacy identity pair with D2B2=%d failed\n",
                    added);
        }
    }
    sixel_cms_close_profile(target);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* A relative affine endpoint with a known neutral black. A positive gain
 * builds the inverse; gain two distinguishes it from the legacy identity. */
static void
iccn_bpc_relative_endpoint(unsigned char tag[84], double black, double gain)
{
    static double const white[3] = {0.9642, 1.0, 0.8249};
    size_t i;
    size_t c;
    double value;

    memset(tag, 0, 84u);
    memcpy(tag, "mpet", 4u);
    tag[9] = tag[11] = 3u;
    iccn_be32(tag + 12u, 1u);
    iccn_be32(tag + 16u, 24u);
    iccn_be32(tag + 20u, 60u);
    memcpy(tag + 24u, "matf", 4u);
    tag[33] = tag[35] = 3u;
    for (i = 0u; i < 3u; ++i) {
        for (c = 0u; c < 3u; ++c) {
            value = i != c ? 0.0 : gain > 0.0
                ? gain / (white[i] * (1.0 - black))
                : white[i] * (1.0 - black);
            iccn_mpe_write_float(tag + 36u + (i * 3u + c) * 4u,
                                (float)value);
        }
        value = gain > 0.0 ? -gain * black / (1.0 - black) : white[i] * black;
        iccn_mpe_write_float(tag + 72u + i * 4u, (float)value);
    }
}

int
icc_numeric_bpc_relative_before_cached_pair_test(int byte_input)
{
    static unsigned int const white[3] = {63190u, 65536u, 54061u};
    static unsigned char const rgb8[3] = {32u, 64u, 96u};
    static unsigned char const expected8[3] = {64u, 128u, 192u};
    static float const rgb[3] = {0.125f, 0.25f, 0.375f};
    static double const expected[3] = {0.25, 0.5, 0.75};
    unsigned char tables[312];
    unsigned char base_src[380];
    unsigned char base_dst[380];
    unsigned char src[476];
    unsigned char dst[572];
    unsigned char *tag;
    unsigned char output8[3];
    float output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    sixel_cms_pixel_format_t format;
    int ok;

    /* Saturation can pair A2B2 with B2A0 only after perceptual is reached.
     * Its identity result must not displace the intervening relative path. */
    iccn_absolute_lut_profile(tables, white, '0');
    memcpy(base_src, tables, sizeof(tables));
    memcpy(base_dst, tables, sizeof(tables));
    iccn_be32(base_src, sizeof(base_src));
    iccn_be32(base_dst, sizeof(base_dst));
    memcpy(base_src + 144u, "A2B2", 4u);
    iccn_be32(base_src + 148u, sizeof(tables));
    iccn_be32(base_src + 152u, 68u);
    iccn_identity_a2b(base_src + sizeof(tables));
    memset(base_src + 232u, 0, 48u);
    iccn_be32(base_dst + 136u, sizeof(tables));
    iccn_be32(base_dst + 140u, 68u);
    iccn_identity_a2b(base_dst + sizeof(tables));

    tag = iccn_expand_tag_directory(src, base_src, sizeof(base_src), 1u);
    iccn_be32(src, sizeof(src));
    memcpy(tag, "D2B1", 4u);
    iccn_be32(tag + 4u, 392u);
    iccn_be32(tag + 8u, 84u);
    iccn_bpc_relative_endpoint(src + 392u, 0.25, 0.0);
    tag = iccn_expand_tag_directory(dst, base_dst, sizeof(base_dst), 2u);
    iccn_be32(dst, sizeof(dst));
    memcpy(tag, "D2B1", 4u);
    iccn_be32(tag + 4u, 404u);
    iccn_be32(tag + 8u, 84u);
    memcpy(tag + 12u, "B2D1", 4u);
    iccn_be32(tag + 16u, 488u);
    iccn_be32(tag + 20u, 84u);
    iccn_bpc_relative_endpoint(dst + 404u, 0.125, 0.0);
    iccn_bpc_relative_endpoint(dst + 488u, 0.125, 2.0);

    /* BPC maps (.25 + .75*x) to (.125 + .875*x), then the destination's
     * inverse gives exactly 2*x. Neither skipping nor doubling BPC does so. */
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(src, sizeof(src));
    target = sixel_cms_open_profile_from_mem(dst, sizeof(dst));
    format = byte_input ? SIXEL_CMS_PIXELFORMAT_RGB_8
                        : SIXEL_CMS_PIXELFORMAT_RGB_F32;
    ok = iccn_transform_pixels(source, format, target, format,
        "saturation,relative,perceptual!",
        byte_input ? (void const *)rgb8 : rgb,
        byte_input ? (void *)output8 : output, 1u);
    if (ok) {
        ok = byte_input ? memcmp(output8, expected8, sizeof(output8)) == 0
                        : iccn_float_result(output, expected, 3u);
    }
    if (!ok) {
        fprintf(stderr, "relative BPC lost to a cached legacy pair\n");
    }
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}

int
icc_numeric_mpet_before_later_legacy_test(void)
{
    static float const input[] = {0.25f, 0.375f, 0.5f};
    static double const expected[] = {0.75, 0.75, 0.75};
    unsigned char base[412];
    unsigned char bytes[412u + 12u + 84u];
    unsigned char mpet[516];
    unsigned char *tag;
    unsigned char *matrix;
    float output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    size_t i;
    int ok;

    iccn_identity_matrix_with_b2a0(base);
    base[207] = '2';
    tag = iccn_expand_tag_directory(bytes, base, sizeof(base), 1u);
    iccn_be32(bytes, sizeof(bytes));
    memcpy(tag, "B2D0", 4u);
    iccn_be32(tag + 4u, sizeof(base) + 12u);
    iccn_be32(tag + 8u, 84u);
    iccn_mpet_transform_profile(mpet, 1, '0', 0, 65536u, 0);
    matrix = mpet + 192u;
    memset(matrix + 12u, 0, 36u);
    for (i = 0u; i < 3u; ++i) {
        iccn_mpe_write_float(matrix + 48u + i * 4u, 0.75f);
    }
    memcpy(bytes + sizeof(base) + 12u, mpet + 168u, 84u);

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_create_linear_srgb_profile();
    target = sixel_cms_open_profile_from_mem(bytes, sizeof(bytes));
    ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        "relative,perceptual,saturation!", input, output, 1u) &&
        iccn_float_result(output, expected, 3u);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_tag_share_budget_test(int distinct)
{
    enum { grid = 64, samples = grid * grid * grid * 3,
           clut_size = 28 + samples * 4, mpet_size = 24 + clut_size };
    static double const input[] = {0.25, 0.5, 0.75};
    static double const relative[] = {
        0.170795938812887680, 0.195230144127393545, 0.418609569483359935
    };
    static double const clut_xyz[] = {0.25, 0.375, 0.5};
    unsigned char *bytes;
    unsigned char *tag;
    unsigned char *mpet;
    unsigned char *clut;
    unsigned char *value;
    sixel_icc_profile_t profile;
    sixel_icc_issue_t issue;
    double xyz[3];
    size_t offset;
    size_t length;
    size_t i;
    int parsed;
    int ok;

    offset = sizeof(color_icc) + 24u;
    length = offset + (distinct ? 2u : 1u) * mpet_size;
    bytes = calloc(1u, length);
    if (bytes == NULL) {
        return EXIT_FAILURE;
    }
    tag = iccn_expand_tag_directory(bytes, color_icc, sizeof(color_icc), 2u);
    iccn_be32(bytes, (unsigned int)length);
    memcpy(tag, "D2B0", 4u);
    iccn_be32(tag + 4u, (unsigned int)offset);
    iccn_be32(tag + 8u, mpet_size);
    memcpy(tag + 12u, "D2B2", 4u);
    iccn_be32(tag + 16u, (unsigned int)offset + (distinct ? mpet_size : 0u));
    iccn_be32(tag + 20u, mpet_size);
    mpet = bytes + offset;
    memcpy(mpet, "mpet", 4u);
    mpet[9] = mpet[11] = 3u;
    iccn_be32(mpet + 12u, 1u);
    iccn_mpet_position(mpet, 0u, 24u, clut_size);
    clut = mpet + 24u;
    memcpy(clut, "clut", 4u);
    clut[9] = clut[11] = 3u;
    memset(clut + 12u, grid, 3u);
    value = clut + 28u;
    for (i = 0u; i < samples; ++i, value += 4u) {
        iccn_mpe_write_float(value, (float)clut_xyz[i % 3u]);
    }
    if (distinct) {
        memcpy(mpet + mpet_size, mpet, mpet_size);
    }

    parsed = sixel_icc_parse_profile_ex(bytes, length, &profile, &issue);
    free(bytes);
    if (distinct || !parsed) {
        if (parsed) {
            sixel_icc_profile_destroy(&profile);
        }
        if (!distinct) {
            fprintf(stderr, "shared MPE failed to parse: issue %d\n", issue);
        }
        return distinct && !parsed && issue == SIXEL_ICC_ISSUE_RESOURCE_LIMIT
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* The relative slot still uses the base fixture's colorants and
     * curves. Each aliased slot must run after the input bytes are freed. */
    ok = issue == SIXEL_ICC_ISSUE_NONE &&
        profile.d2b_mpet[0].elements == profile.d2b_mpet[2].elements &&
        sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(
            xyz, input, 3u, &profile, 1u, 1);
    for (i = 0u; ok && i < 3u; ++i) {
        ok = fabs(xyz[i] - relative[i]) < 1e-4;
    }
    for (i = 0u; ok && i <= 2u; i += 2u) {
        ok = sixel_icc_apply_device_to_physical_xyz(
            xyz, input, 3u, &profile, (unsigned int)i) &&
            memcmp(xyz, clut_xyz, sizeof(xyz)) == 0;
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_downstream_fallback_test(int absolute)
{
    static float const input[] = {0.25f, 0.375f, 0.5f};
    static double const expected[][3] = {
        {65535.0 / 65536.0, 0.375 * 65535.0 / 32768.0,
         0.5 * 65535.0 / 32768.0},
        {-0.06825380, 0.49057835, 0.63499516}
    };
    unsigned char src[516];
    unsigned char dst[516];
    unsigned char bytes[516u + 12u + 84u];
    unsigned char *matrix;
    unsigned char *tag;
    size_t src_size;
    size_t dst_size;
    float output[3];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_engine_t previous;
    int ok;

    src_size = iccn_mpet_transform_profile(src, 0, '1', 0, 65536u, 1);
    iccn_be32(src + iccn_read32(src + 148u) + 8u, 63190u);
    iccn_be32(src + iccn_read32(src + 148u) + 16u, 54061u);
    matrix = src + 192u;
    memset(matrix + 12u, 0, 48u);
    iccn_mpe_write_float(matrix + 48u, FLT_MAX);
    if (absolute) {
        /* Keep relative D2B1 and add absolute D2B3 with huge but finite
         * XYZ. The analytic RGB endpoint cannot store its red channel. */
        tag = iccn_expand_tag_directory(bytes, src, src_size, 1u);
        memcpy(tag, "D2B3", 4u);
        iccn_be32(tag + 4u, (unsigned int)src_size + 12u);
        iccn_be32(tag + 8u, 84u);
        memcpy(bytes + src_size + 12u, src + 168u, 84u);
        src_size += 96u;
        iccn_be32(bytes, (unsigned int)src_size);
        matrix = bytes + 204u;
        memset(matrix + 12u, 0, 48u);
        iccn_mpe_write_float(matrix + 12u, 1.0f);
        iccn_mpe_write_float(matrix + 28u, 1.0f);
        iccn_mpe_write_float(matrix + 44u, 1.0f);
    } else {
        memcpy(bytes, src, src_size);
    }
    dst_size = iccn_mpet_transform_profile(dst, 1, '1', 0, 65536u, 0);
    /* Reverse MPE rejects the huge forward result, but accepts A2B1. */
    iccn_mpe_write_float(dst + 204u, 2.0f);
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(bytes, src_size);
    target = absolute ? sixel_cms_create_linear_srgb_profile()
        : sixel_cms_open_profile_from_mem(dst, dst_size);
    ok = iccn_transform_pixels(source, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        absolute ? "absolute!" : "relative!", input, output, 1u) &&
        iccn_float_result(output, expected[absolute], 3u);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_small_residual_test(unsigned int clut_axes)
{
    static double const input[] = {1.0, 0x1p-60, 1.0};
    unsigned char bytes[468];
    unsigned char *tag;
    unsigned char *element;
    sixel_icc_profile_t profile;
    double result[3];
    unsigned int matrix_size;
    unsigned int clut_size;
    unsigned int clut_points;
    size_t tag_size;
    size_t size;
    int ok;

    memset(bytes, 0, sizeof(bytes));
    clut_points = 1u << clut_axes;
    clut_size = 28u + 4u * clut_points;
    matrix_size = clut_axes > 0u ? 12u + 16u * clut_axes : 28u;
    tag_size = clut_axes > 0u ? 40u + matrix_size + clut_size + 36u : 96u;
    size = 144u + tag_size;
    iccn_be32(bytes, (unsigned int)size);
    bytes[8] = 4u;
    memcpy(bytes + 12u, "scnrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 128u, 1u);
    memcpy(bytes + 132u, "D2B1", 4u);
    iccn_be32(bytes + 136u, 144u);
    iccn_be32(bytes + 140u, (unsigned int)tag_size);
    tag = bytes + 144u;
    memcpy(tag, "mpet", 4u);
    tag[9] = tag[11] = 3u;
    iccn_be32(tag + 12u, clut_axes > 0u ? 3u : 2u);
    /* Reserve 40 bytes for the position table when a CLUT is included. */
    element = tag + (clut_axes > 0u ? 40u : 32u);
    iccn_mpet_position(tag, 0u, (unsigned int)(element - tag), matrix_size);
    memcpy(element, "matf", 4u);
    element[9] = 3u;
    element[11] = clut_axes > 0u ? (unsigned char)clut_axes : 1u;
    /* Select G for the first CLUT axis and leave any other axes at zero.
     * The matrix-only case adds R+G-B. */
    iccn_mpe_write_float(element + 16u, 1.0f);
    if (clut_axes == 0u) {
        iccn_mpe_write_float(element + 12u, 1.0f);
        iccn_mpe_write_float(element + 20u, -1.0f);
    }
    element += matrix_size;
    if (clut_axes > 0u) {
        iccn_mpet_position(tag, 1u, (unsigned int)(element - tag), clut_size);
        memcpy(element, "clut", 4u);
        element[9] = (unsigned char)clut_axes;
        element[11] = 1u;
        memset(element + 12u, 2, clut_axes);
        /* (1 - 2^-60) * 2^-60 - 2^-60 is exactly -2^-120. */
        iccn_mpe_write_float(element + 28u, 0x1p-60f);
        iccn_mpe_write_float(element + 28u + 2u * clut_points, -1.0f);
        element += clut_size;
    }
    iccn_mpet_position(tag, clut_axes > 0u ? 2u : 1u,
        (unsigned int)(element - tag), 36u);
    memcpy(element, "matf", 4u);
    element[9] = 1u;
    element[11] = 3u;
    iccn_mpe_write_float(element + 12u, clut_axes > 0u ? 0x1p100f : 0x1p60f);
    ok = sixel_icc_parse_profile(bytes, size, &profile);
    if (ok) {
        ok = sixel_icc_device_to_pcs(result, input, 3u, &profile, 1u, 0)
                == SIXEL_ICC_PATH_MPET &&
            result[0] == (clut_axes > 0u ? -0x1p-20 : 1.0) &&
            result[1] == 0.0 && result[2] == 0.0;
        sixel_icc_profile_destroy(&profile);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_matrix_product_residual_test(void)
{
    static double const input[] = {0.5, 0.5 + 0x1p-24, 0.5};
    unsigned char bytes[288] = {0};
    unsigned char *tag;
    unsigned char *matrix;
    sixel_icc_profile_t profile;
    double result[3] = {0.0, 0.0, 0.0};
    int path;
    int ok;

    iccn_be32(bytes, sizeof(bytes));
    bytes[8] = 4u;
    memcpy(bytes + 12u, "scnrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 128u, 1u);
    memcpy(bytes + 132u, "D2B1", 4u);
    iccn_be32(bytes + 136u, 144u);
    iccn_be32(bytes + 140u, sizeof(bytes) - 144u);
    tag = bytes + 144u;
    memcpy(tag, "mpet", 4u);
    tag[9] = tag[11] = 3u;
    iccn_be32(tag + 12u, 3u);
    iccn_mpet_position(tag, 0u, 40u, 44u);
    iccn_mpet_position(tag, 1u, 84u, 24u);
    iccn_mpet_position(tag, 2u, 108u, 36u);
    matrix = tag + 40u;
    memcpy(matrix, "matf", 4u);
    matrix[9] = 3u;
    matrix[11] = 2u;
    iccn_mpe_write_float(matrix + 12u, 1.0f);
    iccn_mpe_write_float(matrix + 28u, 1.0f);
    iccn_mpe_write_float(matrix + 36u, 0x1p-31f);
    iccn_mpe_write_float(matrix + 40u, 0x1p-31f);
    matrix = tag + 84u;
    memcpy(matrix, "matf", 4u);
    matrix[9] = 2u;
    matrix[11] = 1u;
    iccn_mpe_write_float(matrix + 12u, 1.0f + 0x1p-23f);
    iccn_mpe_write_float(matrix + 16u, -1.0f);
    matrix = tag + 108u;
    memcpy(matrix, "matf", 4u);
    matrix[9] = 1u;
    matrix[11] = 3u;
    iccn_mpe_write_float(matrix + 12u, 0x1p54f);

    /* (0.5 + 2^-31) * (1 + 2^-23) - (0.5 + 2^-24 + 2^-31)
     * leaves only 2^-31 * 2^-23, which the final matrix scales to one. */
    if (!sixel_icc_parse_profile(bytes, sizeof(bytes), &profile)) {
        return EXIT_FAILURE;
    }
    path = sixel_icc_device_to_pcs(result, input, 3u, &profile, 1u, 0);
    ok = path == SIXEL_ICC_PATH_MPET &&
        result[0] == 1.0 && result[1] == 0.0 && result[2] == 0.0;
    if (!ok) {
        fprintf(stderr, "matrix product: path=%d, result=(%.17g, %.17g, "
            "%.17g), expected=(1, 0, 0)\n", path,
            result[0], result[1], result[2]);
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_curve_small_residual_test(void)
{
    static unsigned int const lower[] = {0x3f800000u, 0u, 0u, 0x21800000u};
    static unsigned int const upper[] = {0x3f800000u, 0u, 0u, 0xbf800000u};
    static double const input[] = {1.0, 0x1p-60, 1.0};
    unsigned char bytes[364] = {0};
    unsigned char *tag;
    unsigned char *curves;
    unsigned char *curve;
    unsigned char *matrix;
    sixel_icc_profile_t profile;
    double result[3];
    size_t i;
    int ok;

    iccn_be32(bytes, sizeof(bytes));
    bytes[8] = 4u;
    memcpy(bytes + 12u, "scnrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    iccn_be32(bytes + 128u, 1u);
    memcpy(bytes + 132u, "D2B1", 4u);
    iccn_be32(bytes + 136u, 144u);
    iccn_be32(bytes + 140u, 220u);
    tag = bytes + 144u;
    memcpy(tag, "mpet", 4u);
    tag[9] = tag[11] = 3u;
    iccn_be32(tag + 12u, 2u);
    iccn_mpet_position(tag, 0u, 32u, 128u);
    iccn_mpet_position(tag, 1u, 160u, 60u);
    curves = tag + 32u;
    memcpy(curves, "cvst", 4u);
    curves[9] = curves[11] = 3u;
    for (i = 0u; i < 3u; ++i) {
        iccn_be32(curves + 12u + 8u * i, 36u);
        iccn_be32(curves + 16u + 8u * i, 92u);
    }
    curve = curves + 36u;
    memcpy(curve, "curf", 4u);
    curve[9] = 3u;
    iccn_mpe_write_float(curve + 16u, 1.0f);
    iccn_mpe_write_formula(curve + 20u, 0u, lower);
    memcpy(curve + 48u, "samf", 4u);
    iccn_be32(curve + 56u, 1u);
    iccn_mpe_write_float(curve + 60u, -1.0f);
    iccn_mpe_write_formula(curve + 64u, 0u, upper);

    /* At G=2^-60, interpolation from 2^-60 to -1 gives -2^-120.
     * The following matrix exposes the residual by scaling G by 2^100. */
    matrix = tag + 160u;
    memcpy(matrix, "matf", 4u);
    matrix[9] = matrix[11] = 3u;
    iccn_mpe_write_float(matrix + 16u, 0x1p100f);
    if (!sixel_icc_parse_profile(bytes, sizeof(bytes), &profile)) {
        return EXIT_FAILURE;
    }
    ok = sixel_icc_device_to_pcs(result, input, 3u, &profile, 1u, 0)
            == SIXEL_ICC_PATH_MPET &&
        result[0] == -0x1p-20 && result[1] == 0.0 && result[2] == 0.0;
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_mpet_palette_test(int reverse, char slot, int gamut)
{
    static double const input[] = {0.2, 0.3, 0.4};
    unsigned char bytes[sizeof(color_icc) + 96u];
    unsigned char mpet[516];
    unsigned char *tag;
    sixel_color_def_t *definition;
    sixel_color_def_t *reference;
    sixel_snap_grid_t *grid;
    sixel_snap_grid_t *base_grid;
    sixel_gamut_t *mapper;
    sixel_gamut_t *base_mapper;
    sixel_allocator_t *allocator;
    double result[3];
    double expected[3];
    int ok;

    tag = iccn_expand_tag_directory(bytes, color_icc, sizeof(color_icc), 1u);
    iccn_be32(bytes, sizeof(bytes));
    memcpy(tag, reverse ? "B2D" : "D2B", 3u);
    tag[3] = (unsigned char)slot;
    iccn_be32(tag + 4u, sizeof(color_icc) + 12u);
    iccn_be32(tag + 8u, 84u);
    iccn_mpet_transform_profile(mpet, reverse, slot, 0, 65536u, 0);
    memcpy(bytes + sizeof(color_icc) + 12u, mpet + 168u, 84u);
    definition = sixel_cdef_icc(bytes, sizeof(bytes));
    reference = sixel_cdef_icc(color_icc, sizeof(color_icc));
    allocator = NULL;
    grid = base_grid = NULL;
    mapper = base_mapper = NULL;
    ok = definition != NULL && reference != NULL &&
        SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
                                               NULL));
    if (ok && slot == '1') {
        /* A relative MPE replaces part of the matrix/TRC transform. */
        ok = gamut
            ? SIXEL_FAILED(sixel_gamut_new(&mapper, definition, 0,
                SIXEL_GAMUT_LOCAL_MINDE, allocator))
            : SIXEL_FAILED(sixel_snap_grid_new(&grid, SIXEL_SNAP_GRID_DIRECT,
                SIXEL_COLORSPACE_LINEAR, definition, allocator));
    } else if (gamut && ok) {
        ok = SIXEL_SUCCEEDED(sixel_gamut_new(&mapper, definition, 0,
                SIXEL_GAMUT_LOCAL_MINDE, allocator)) &&
            SIXEL_SUCCEEDED(sixel_gamut_new(&base_mapper, reference, 0,
                SIXEL_GAMUT_LOCAL_MINDE, allocator)) &&
            sixel_gamut_apply(mapper, input, result, SIXEL_COLORSPACE_LINEAR) &&
            sixel_gamut_apply(base_mapper, input, expected,
                               SIXEL_COLORSPACE_LINEAR);
    } else if (ok) {
        ok = SIXEL_SUCCEEDED(sixel_snap_grid_new(&grid, SIXEL_SNAP_GRID_DIRECT,
                SIXEL_COLORSPACE_LINEAR, definition, allocator)) &&
            SIXEL_SUCCEEDED(sixel_snap_grid_new(&base_grid,
                SIXEL_SNAP_GRID_DIRECT, SIXEL_COLORSPACE_LINEAR, reference,
                allocator)) &&
            SIXEL_SUCCEEDED(sixel_snap_grid_project(grid, input, 1, 1.0,
                                                       result)) &&
            SIXEL_SUCCEEDED(sixel_snap_grid_project(base_grid, input, 1, 1.0,
                                                       expected));
    }
    ok = ok && (slot == '1' || memcmp(result, expected, sizeof(result)) == 0);
    sixel_snap_grid_destroy(base_grid);
    sixel_snap_grid_destroy(grid);
    sixel_gamut_free(base_mapper);
    sixel_gamut_free(mapper);
    sixel_allocator_unref(allocator);
    sixel_cdef_free(reference);
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
icc_numeric_nonrelative_mpet_palette_test(int gamut)
{
    static char const slots[] = {'0', '2', '1'};
    size_t i;
    int reverse;

    for (i = 0u; i < sizeof(slots); ++i) {
        for (reverse = 0; reverse <= 1; ++reverse) {
            if (icc_numeric_mpet_palette_test(reverse, slots[i], gamut)
                    != EXIT_SUCCESS) {
                fprintf(stderr, "%s%c: unexpected relative operation\n",
                        reverse ? "B2D" : "D2B", slots[i]);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

typedef struct iccn_png_capture {
    float values[9];
    unsigned int calls;
    int unprofiled;
} iccn_png_capture_t;

static SIXELSTATUS
iccn_capture_mpet_png(sixel_frame_t *frame, void *context)
{
    iccn_png_capture_t *capture;
    unsigned char const *pixels;
    double value;
    size_t i;

    capture = context;
    ++capture->calls;
    if (capture->unprofiled && frame->width == 3 && frame->height == 1 &&
        sixel_frame_format(frame) == SIXEL_PIXELFORMAT_RGB888) {
        pixels = sixel_frame_get_pixels(frame);
        for (i = 0u; i < 9u; ++i) {
            value = pixels[i] / 255.0;
            capture->values[i] = (float)(value <= 0.04045
                ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4));
        }
        return SIXEL_OK;
    }
    if (frame->width != 3 || frame->height != 1 ||
        sixel_frame_format(frame) != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->format.coordinates != SIXEL_COLORSPACE_LINEAR) {
        fprintf(stderr, "PNG frame %dx%d format=%x space=%d\n",
            frame->width, frame->height, sixel_frame_format(frame),
            frame->format.coordinates);
        return SIXEL_BAD_INPUT;
    }
    memcpy(capture->values, sixel_frame_get_pixels(frame),
           sizeof(capture->values));
    return SIXEL_OK;
}

int
icc_numeric_mpet_png_storage_fallback_test(void)
{
    static struct {
        unsigned char const *bytes;
        size_t size;
    } const fixtures[] = {
        {iccn_png_overflow, sizeof(iccn_png_overflow)},
        {iccn_png_control, sizeof(iccn_png_control)},
        {iccn_png_unprofiled, sizeof(iccn_png_unprofiled)},
        {iccn_png_finite, sizeof(iccn_png_finite)}
    };
    /* Inverse Bradford-adapted sRGB matrix applied to the legacy identity
     * A2B1 XYZ=(RGB8/255)*(65535/32768), independently computed offline. */
    static double const expected[] = {
        -0.1370408339536210, 0.9849893630809516, 1.2749511912846483,
        -0.0559425315684849, 0.1344124041163471, 0.6859170612232661,
        0.2801083577064768, 0.7645194267287527, 2.3514461693201323
    };
    iccn_png_capture_t captures[4];
    loader_probe_callback_state_t callback;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *loader;
    sixel_chunk_t *chunk;
    size_t i;
    int engine;
    int one;
    int ok;

    allocator = NULL;
    loader = NULL;
    chunk = NULL;
    engine = SIXEL_CMS_ENGINE_BUILTIN;
    one = 1;
    memset(captures, 0, sizeof(captures));
    captures[2].unprofiled = 1;
    ok = sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT",
                             "relative!") == 0 &&
        SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
                                               NULL)) &&
        SIXEL_SUCCEEDED(create_loader_component_by_name("builtin", allocator,
                                                         (void **)&loader));
    if (ok) {
        ok = SIXEL_SUCCEEDED(sixel_loader_component_setopt(loader,
                SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine)) &&
            SIXEL_SUCCEEDED(sixel_loader_component_setopt(loader,
                SIXEL_LOADER_COMPONENT_OPTION_PREFER_FLOAT32, &one));
    }
    for (i = 0u; ok && i < 4u; ++i) {
        callback = (loader_probe_callback_state_t){
            NULL, iccn_capture_mpet_png, &captures[i]
        };
        ok = SIXEL_SUCCEEDED(sixel_chunk_create_from_memory(&chunk,
            fixtures[i].bytes, fixtures[i].size, NULL, allocator));
        if (ok) {
            ok = SIXEL_SUCCEEDED(sixel_loader_component_load(loader, chunk,
                capture_frame_trampoline, &callback)) &&
                captures[i].calls == 1u;
            chunk->vtbl->unref(chunk);
            chunk = NULL;
        }
        if (!ok) {
            fprintf(stderr, "PNG storage fixture %zu failed: %s\n", i,
                sixel_helper_get_additional_message());
        }
    }
    /* Both controls distinguish correct ICC application from an ignored
     * profile or accidental selection of a different MPE. */
    ok = ok && iccn_float_result(captures[0].values, expected, 9u) &&
        memcmp(captures[0].values, captures[1].values,
               sizeof(captures[0].values)) == 0 &&
        memcmp(captures[0].values, captures[2].values,
               sizeof(captures[0].values)) != 0 &&
        memcmp(captures[0].values, captures[3].values,
               sizeof(captures[0].values)) != 0;
    if (loader != NULL) {
        sixel_loader_component_unref(loader);
    }
    sixel_allocator_unref(allocator);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
