/* SPDX-License-Identifier: MIT */
/* The shared domain reason is distinct from native layout rejection. */
#ifndef TEST_PROFILE_FORMAT_SCOPE_COMMON_H
#define TEST_PROFILE_FORMAT_SCOPE_COMMON_H
#include "tests/processing/cms/profile_format_common.h"

static int
profile_format_outside(int engine, int format, int destination)
{
    unsigned char const samples[2] = { 26u, 253u };
    unsigned char input[128];
    unsigned char original[sizeof(input)];
    unsigned char output[sizeof(input)];
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    size_t i;
    size_t channels;
    int applied;
    int legacy;
    int ok;

    legacy = destination != SIXEL_CMS_PIXELFORMAT_RGB_F32;
    source = sixel_cms_create_srgb_profile();
    target = legacy ? sixel_cms_create_srgb_profile()
                    : sixel_cms_create_linear_srgb_profile();
    memset(input, 0xa5, sizeof(input));
    memset(output, 0xa5, sizeof(output));
    memcpy(input + 32u, samples, sizeof(samples));
    memcpy(original, input, sizeof(original));
    transform = sixel_cms_create_transform(source, format, target,
        destination, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = source != NULL && target != NULL;
    /* Invalid formats retain builtin apply failure and ColorSync creation
     * failure. The byte convenience paths retain exact neutral bytes. */
    ok = ok && (engine == SIXEL_CMS_ENGINE_COLORSYNC
        ? transform == NULL
        : (int)sixel_cms_transform_engine(transform) == engine);
    fprintf(stderr, "PFTRACE|kind=outside|engine=%d|src=%d\n",
        engine, format);
    applied = transform == NULL ? 0 : sixel_cms_do_transform(transform,
        input + 32u, output + 32u, 2u);
    fprintf(stderr, "PFTRACE|kind=end\n");
    ok = ok && applied == legacy &&
         memcmp(input, original, sizeof(original)) == 0;
    channels = destination == SIXEL_CMS_PIXELFORMAT_RGB_8 ? 3u : 1u;
    for (i = 0u; i < sizeof(output); ++i) {
        if (legacy && i >= 32u && i < 32u + 2u * channels) {
            ok = ok && output[i] == samples[(i - 32u) / channels];
        } else {
            ok = ok && output[i] == 0xa5;
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(source);
    sixel_cms_close_profile(target);
    return ok;
}

static int
profile_format_unknown(void)
{
    int const rejected_formats[3] = { 1, 5, 6 };
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    float input[6];
    float before[6];
    float output[14];
    unsigned char original[sizeof(output)];
    double const (*bounds)[3];
    size_t pixel;
    size_t channel;
    size_t i;
    size_t specimen;
    int format;
    int expected;
    int ok;

    source = profile_format_open(7);
    target = profile_format_open(4);
    ok = source != NULL && target != NULL &&
        sixel_cms_get_color_space(source) == SIXEL_CMS_COLORSPACE_UNKNOWN;
    for (pixel = 0u; pixel < 2u; ++pixel) {
        for (channel = 0u; channel < 3u; ++channel) {
            input[pixel * 3u + channel] =
                (float)pf_xyz_source[pixel][channel];
        }
    }
    memcpy(before, input, sizeof(before));
    memset(output, 0xa5, sizeof(output));
    memcpy(original, output, sizeof(original));
    transform = sixel_cms_create_transform(source,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, target,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    fprintf(stderr, "PFTRACE|kind=outside|engine=4|src=4\n");
    ok = ok && sixel_cms_transform_engine(transform) ==
        SIXEL_CMS_ENGINE_COLORSYNC &&
        sixel_cms_do_transform(transform, input, output + 4u, 2u);
    fprintf(stderr, "PFTRACE|kind=end\n");
    ok = ok && memcmp(input, before, sizeof(before)) == 0;
    for (pixel = 0u; ok && pixel < 2u; ++pixel) {
        bounds = pf_xyz_bounds[pixel][0];
        for (channel = 0u; channel < 3u; ++channel) {
            if (!isfinite(output[4u + pixel * 3u + channel]) ||
                output[4u + pixel * 3u + channel] < bounds[0][channel] ||
                output[4u + pixel * 3u + channel] > bounds[1][channel]) {
                ok = 0;
            }
        }
    }
    for (i = 0u; i < sizeof(output); ++i) {
        if ((i < 4u * sizeof(float) || i >= 10u * sizeof(float)) &&
            ((unsigned char *)output)[i] != original[i]) {
            ok = 0;
        }
    }
    sixel_cms_delete_transform(transform);
    /* These unequal UNKNOWN layouts remain owned by the native channel
     * guard. The TAP observer must see that reason, never the new domain
     * reason. Small counts also keep bypass comparisons within scratch. */
    for (i = 0u; ok && i < 3u; ++i) {
        format = rejected_formats[i];
        expected = format == SIXEL_CMS_PIXELFORMAT_GRAY_8 ? 1 : 4;
        specimen = 36u + ((size_t)format - 1u) * 2u;
        transform = sixel_cms_create_transform(source, format, target,
            SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
        fprintf(stderr, "PFTRACE|kind=outside|engine=4|src=%d|"
            "native=3|expected=%d\n", format, expected);
        ok = sixel_cms_transform_engine(transform) ==
            SIXEL_CMS_ENGINE_COLORSYNC &&
            profile_format_call(transform, specimen, 2u, 0, 1);
        fprintf(stderr, "PFTRACE|kind=end\n");
        sixel_cms_delete_transform(transform);
    }
    sixel_cms_close_profile(source);
    sixel_cms_close_profile(target);
    return ok;
}

static int
profile_format_scope(int engine)
{
    int const invalid_formats[3] = { 0, 10, -1 };
    sixel_cms_engine_t previous;
    size_t specimen;
    size_t i;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    ok = 1;
    /* Reuse the independent positives and every known mismatch. Their
     * call boundaries let TAP require a reason only on the owning calls,
     * including formats already rejected by another backend/layout check. */
    for (specimen = 0u; ok && specimen < 54u; ++specimen) {
        if (pf_cases[specimen].engine == engine) {
            ok = profile_format_test(specimen) == 0;
        }
    }
    for (i = 0u; ok && i < 3u; ++i) {
        ok = profile_format_outside(engine, invalid_formats[i],
                                       SIXEL_CMS_PIXELFORMAT_RGB_F32);
    }
    if (ok && engine == SIXEL_CMS_ENGINE_BUILTIN) {
        ok = profile_format_outside(engine, SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                      SIXEL_CMS_PIXELFORMAT_GRAY_8) &&
             profile_format_outside(engine, SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                      SIXEL_CMS_PIXELFORMAT_RGB_8);
    } else if (ok) {
        ok = profile_format_unknown();
    }
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "PFSCOPE|engine=%d|known-and-excluded-verified\n",
            engine);
    }
    return ok ? 0 : 1;
}
#endif
