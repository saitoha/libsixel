/* SPDX-License-Identifier: MIT */
/* Reuse recorded source words, profiles and independent ideal values.
 * No native output is saved as an ideal for the extended device domain. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/compat_stub.h"
#include "tests/data/color-numeric/curv-samples.h"
#include "tests/data/color-numeric/gray-samples.h"
#include "tests/data/color-numeric/rgb-power.h"
#include "tests/data/color-numeric/icc.h"
#include "mixed_domain_common.h"

void
cms_mixed_fixture(int kind, cms_mixed_fixture_t *f)
{
    static float const extended[4][3] = {
        {-0.001f, -0.001f, -0.001f}, {-0.25f, -0.25f, -0.25f},
        {1.25f, 1.25f, 1.25f}, {0.001f, -0.001f, 1.25f}
    };
    size_t p;
    size_t c;
    int variant;

    memset(f, 0, sizeof(*f));
    f->gray = kind < 2;
    f->swap = kind == 4;
    f->non_target = kind == 5;
    f->normalized = kind == 1 ? COLOR_GRAY_SAMPLE_COUNT : COLOR_CURV_SAMPLES;
    f->count = f->normalized + 4u;
    /* Retain mask6 para0 (g=2,1,1): its original native extended output
     * can depend on neighboring pixels. The paired reference below keeps
     * its original curves while testing that this defect stays fixed. */
    variant = kind == 0 ? 11 : kind == 2 ? 3 : kind == 4 ? 18 : 33;
    if (kind == 0 || kind == 2) {
        f->icc = color_curv_profiles[variant].bytes[0];
        f->length = color_curv_profiles[variant].length[0];
    } else if (kind == 1) {
        f->icc = color_gray_icc;
        f->length = sizeof(color_gray_icc);
    } else if (kind == 5) {
        f->icc = color_icc;
        f->length = sizeof(color_icc);
    } else {
        f->icc = color_rgb_power_profiles[variant].bytes;
        f->length = color_rgb_power_profiles[variant].length;
    }
    for (p = 0u; p < f->normalized; ++p) {
        for (c = 0u; c < 3u; ++c) {
            f->input[p * 3u + c] = (float)((kind == 1
                ? color_gray_source[p]
                : color_curv_source[p][f->gray ? 0u : c]) / 65535.0);
            f->ideal[p * 3u + c] = kind == 1 ? color_gray_linear[p]
                : kind == 0 || kind == 2
                ? color_curv_profiles[variant].linear[p][c]
                : kind == 5 ? 0.0
                : color_rgb_power_profiles[variant].linear[p][c];
        }
    }
    for (p = 0u; p < 4u; ++p) {
        for (c = 0u; c < 3u; ++c) {
            /* Gray's fourth witness must also have an outside component. */
            f->input[(f->normalized + p) * 3u + c] =
                extended[p][f->gray ? 1u : c];
        }
    }
}

static unsigned int
md_read32(unsigned char const *p)
{
    return ((unsigned int)p[0] * 16777216u) |
        ((unsigned int)p[1] * 65536u) | ((unsigned int)p[2] * 256u) | p[3];
}

static unsigned char const *
md_tag(unsigned char const *icc, char const *name, size_t *length)
{
    unsigned int i;
    unsigned char const *entry;

    for (i = 0u; i < md_read32(icc + 128); ++i) {
        entry = icc + 132u + i * 12u;
        if (memcmp(entry, name, 4u) == 0) {
            *length = md_read32(entry + 8);
            return icc + md_read32(entry + 4);
        }
    }
    *length = 0u;
    return NULL;
}

/* The selected mixed profile has a known red/blue exchange. Swap its tag
 * table references and sample words together, without calling preparation. */
static int
md_swap(unsigned char *icc, float *pixels, size_t count)
{
    unsigned char entry[8];
    unsigned char *red;
    unsigned char *blue;
    float value;
    size_t i;
    int pair;

    for (pair = 0; pair < 2; ++pair) {
        red = blue = NULL;
        for (i = 0u; i < md_read32(icc + 128); ++i) {
            if (memcmp(icc + 132u + i * 12u,
                       pair ? "rXYZ" : "rTRC", 4u) == 0) {
                red = icc + 136u + i * 12u;
            }
            if (memcmp(icc + 132u + i * 12u,
                       pair ? "bXYZ" : "bTRC", 4u) == 0) {
                blue = icc + 136u + i * 12u;
            }
        }
        if (red == NULL || blue == NULL) { return 0; }
        memcpy(entry, red, sizeof(entry));
        memcpy(red, blue, sizeof(entry));
        memcpy(blue, entry, sizeof(entry));
    }
    memset(icc + 84, 0, 16);
    for (i = 0u; i < count; ++i) {
        memcpy(&value, pixels + i * 3u, sizeof(value));
        memcpy(pixels + i * 3u, pixels + i * 3u + 2u, sizeof(value));
        memcpy(pixels + i * 3u + 2u, &value, sizeof(value));
    }
    return 1;
}

/* Compare each actual native-boundary record in TAP. The curve spelling is
 * constructed from stored integers, independently of the adapter routines. */
static int
md_expected(cms_mixed_fixture_t const *f, int mode, int group)
{
    static char const *const names[8] = {
        "rTRC", "gTRC", "bTRC", "rXYZ", "gXYZ", "bXYZ", "wtpt", "kTRC"
    };
    unsigned char icc[1024];
    unsigned char curve[32];
    unsigned char const *tag;
    float samples[CMS_MIXED_MAX * 3u];
    uint32_t word;
    unsigned int gamma;
    size_t length;
    size_t count;
    size_t offset;
    size_t i;
    size_t j;
    int prepared;

    prepared = !f->non_target && (mode == 0 || f->swap);
    if (group == 2 && !prepared) { return 1; }
    offset = group == 2 ? f->normalized : 0u;
    count = group == 2 ? 4u : prepared ? f->normalized : f->count;
    memcpy(icc, f->icc, f->length);
    memcpy(samples, f->input + offset * 3u, count * 3u * sizeof(float));
    if (f->swap && !md_swap(icc, samples, count)) { return 0; }
    fprintf(stderr, "MDEXPECT%d|begin\n", group);
    for (i = f->gray ? 6u : 0u; i < (f->gray ? 8u : 7u); ++i) {
        tag = md_tag(icc, names[i], &length);
        if (tag == NULL || length > 40u) { return 0; }
        if (group == 1 && prepared && mode == 0 && (i < 3u || i == 7u)) {
            gamma = memcmp(tag, "curv", 4u) == 0
                ? ((unsigned int)tag[12] * 256u + tag[13]) * 256u
                : md_read32(tag + 12);
            memset(curve, 0, sizeof(curve));
            memcpy(curve, "para", 4);
            curve[9] = 3u;
            curve[17] = 1u;
            for (j = 0u; j < 4u; ++j) {
                curve[15u - j] = (unsigned char)(gamma % 256u);
                gamma /= 256u;
            }
            tag = curve;
            length = sizeof(curve);
        }
        fprintf(stderr, "libsixel[loader_contract]: "
            "LSXCMS3|event=gamma-source|tag=%s|bytes=", names[i]);
        for (j = 0u; j < length; ++j) { fprintf(stderr, "%02x", tag[j]); }
        fputc('\n', stderr);
    }
    fprintf(stderr, "libsixel[loader_contract]: "
        "LSXCMS3|event=gamma-source|samples=");
    for (i = 0u; i < count * (f->gray ? 1u : 3u); ++i) {
        memcpy(&word, samples + (f->gray ? i * 3u : i), sizeof(word));
        fprintf(stderr, "%08x", (unsigned int)word);
    }
    fprintf(stderr, "\nMDEXPECT%d|end\n", group);
    return 1;
}

static int
md_native(sixel_cms_profile_t *profile, float const *input, float *output,
             size_t count, int gray)
{
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    float packed[CMS_MIXED_MAX];
    size_t i;
    int ok;

    for (i = 0u; i < count; ++i) { packed[i] = input[i * 3u]; }
    destination = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(profile, gray
        ? SIXEL_CMS_PIXELFORMAT_GRAY_F32 : SIXEL_CMS_PIXELFORMAT_RGB_F32,
        destination, SIXEL_CMS_PIXELFORMAT_RGB_F32, 0);
    ok = transform != NULL && sixel_cms_transform_engine(transform) ==
        SIXEL_CMS_ENGINE_COLORSYNC && sixel_cms_do_transform(transform,
            gray ? packed : input, output, count);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(destination);
    return ok;
}

static int
md_equal(float const *a, float const *b, size_t count)
{
    size_t i;

    /* Native batching can change zero's sign, not its color value. */
    for (i = 0u; i < count; ++i) {
        if (!isfinite(a[i]) || !isfinite(b[i]) || a[i] != b[i]) {
            fprintf(stderr, "MDVALUE|component=%zu|a=%.17g|b=%.17g\n",
                i, a[i], b[i]);
            return 0;
        }
    }
    return 1;
}

static int
md_convert(cms_mixed_fixture_t const *f, sixel_cms_profile_t *profile,
               float *pixels, size_t count, int strict)
{
    return strict ? sixel_cms_convert_colorsync_linear(pixels, (int)count,
        1, 0, f->icc, f->length) : sixel_cms_convert_profile_to_linearrgb(
        (unsigned char *)pixels, (int)count, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
        profile);
}

static int
md_arrangements(cms_mixed_fixture_t const *f, sixel_cms_profile_t *profile,
                   float const *whole, int strict)
{
    float guarded[CMS_MIXED_MAX * 3u + 2u];
    float ordered[CMS_MIXED_MAX * 3u];
    size_t bytes;
    size_t p;
    size_t offset;
    size_t length;
    int arrangement;
    int ok;

    bytes = f->count * 3u * sizeof(float);
    for (arrangement = 0; arrangement < 3; ++arrangement) {
        guarded[0] = guarded[f->count * 3u + 1u] = 12345.0f;
        for (p = 0u; p < f->count; ++p) {
            memcpy(guarded + 1u + p * 3u,
                f->input + (arrangement == 2 ? f->count - p - 1u : p) * 3u,
                3u * sizeof(float));
        }
        ok = 1;
        offset = 0u;
        while (ok && offset < f->count) {
            length = arrangement == 0 ? 1u : arrangement == 1 && !offset
                ? f->count / 2u : f->count - offset;
            ok = md_convert(f, profile, guarded + 1u + offset * 3u,
                            length, strict);
            offset += length;
        }
        if (!ok || guarded[0] != 12345.0f ||
            guarded[f->count * 3u + 1u] != 12345.0f) { return 0; }
        memcpy(ordered, guarded + 1u, bytes);
        if (arrangement == 2) {
            for (p = 0u; p < f->count; ++p) {
                memcpy(ordered + p * 3u,
                    guarded + 1u + (f->count - p - 1u) * 3u,
                    3u * sizeof(float));
            }
        }
        if (!md_equal(whole, ordered, f->count * 3u)) { return 0; }
    }
    return 1;
}

int
cms_mixed_values(int observation)
{
    static int const kinds[8] = {0, 1, 2, 3, 4, 4, 0, 5};
    static int const stricts[8] = {0, 0, 0, 1, 0, 1, 0, 0};
    cms_mixed_fixture_t f;
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *profile;
    sixel_cms_profile_t *permuted;
    unsigned char original[1024];
    unsigned char exchanged[1024];
    float raw[CMS_MIXED_MAX * 3u];
    float native[CMS_MIXED_MAX * 3u];
    float ordered[CMS_MIXED_MAX * 3u];
    float whole[CMS_MIXED_MAX * 3u];
    float first[CMS_MIXED_MAX * 3u];
    double magnitude;
    double allowance;
    size_t bytes;
    size_t p;
    size_t c;
    int mode;
    int pass;
    int passes;
    int ok;

    if (observation < 0 || observation > 7) { return 1; }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    cms_mixed_fixture(kinds[observation], &f);
    memcpy(original, f.icc, f.length);
    memcpy(exchanged, f.icc, f.length);
    bytes = f.count * 3u * sizeof(float);
    memcpy(ordered, f.input, bytes);
    profile = sixel_cms_open_profile_from_mem(f.icc, f.length);
    permuted = NULL;
    ok = profile != NULL && md_native(profile, f.input, raw, f.count, f.gray);
    if (ok && f.swap) {
        ok = md_swap(exchanged, ordered, f.count);
        permuted = sixel_cms_open_profile_from_mem(exchanged, f.length);
        ok = ok && permuted != NULL && md_native(permuted, ordered, native,
                                                  f.count, 0);
    } else {
        memcpy(native, raw, bytes);
    }
    passes = observation == 4 || observation == 6 ? 3 : 1;
    for (pass = 0; ok && pass < passes; ++pass) {
        mode = observation == 5 || pass == 1;
        ok = sixel_compat_setenv("SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA",
                                 mode ? "1" : "0") == 0;
        /* Inspect compatibility Gray as a whole original batch. */
        if (pass == (observation == 6 ? 1 : 0)) {
            ok = ok && md_expected(&f, mode, 1) && md_expected(&f, mode, 2);
            fprintf(stderr, "MDPHASE|begin\n");
        }
        memcpy(whole, f.input, bytes);
        ok = ok && md_convert(&f, profile, whole, f.count,
                                stricts[observation]);
        if (pass == (observation == 6 ? 1 : 0)) {
            fprintf(stderr, "MDPHASE|end\n");
        }
        for (p = 0u; ok && p < f.count; ++p) {
            if (p >= f.normalized || f.non_target || mode) {
                ok = md_equal(whole + p * 3u,
                    (f.non_target ? raw : native)
                        + p * 3u, 3u);
                continue;
            }
            magnitude = fmax(fabs(f.ideal[p * 3u]),
                fmax(fabs(f.ideal[p * 3u + 1u]),
                     fabs(f.ideal[p * 3u + 2u])));
            allowance = magnitude * color_curv_relative;
            for (c = 0u; c < 3u; ++c) {
                if (!isfinite(whole[p * 3u + c]) ||
                    fabs(whole[p * 3u + c] - f.ideal[p * 3u + c]) >
                        allowance) { ok = 0; }
            }
        }
        ok = ok && md_arrangements(&f, profile, whole, stricts[observation]);
        if (pass == 0) { memcpy(first, whole, bytes); }
        if (pass == 2) { ok = ok && md_equal(first, whole, f.count * 3u); }
        if (ok && observation == 6 && pass == 1) {
            ok = sixel_compat_setenv(
                "SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA", "invalid") == 0;
            memcpy(whole, f.input, bytes);
            ok = ok && !md_convert(&f, profile, whole, f.count, 0) &&
                memcmp(whole, f.input, bytes) == 0;
        }
    }
    ok = ok && memcmp(original, f.icc, f.length) == 0 &&
        md_native(profile, f.input, ordered, f.count, f.gray) &&
        md_equal(raw, ordered, f.count * 3u);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA", "");
    sixel_cms_close_profile(permuted);
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    if (!ok) { fprintf(stderr, "mixed observation %d failed\n", observation); }
    return ok ? 0 : 1;
}
