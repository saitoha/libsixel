/* SPDX-License-Identifier: MIT */
/* Independent separate values must pass before exact-alias rejection. */
#ifndef TEST_LCMS_EXPANDING_ALIAS_COMMON_H
#define TEST_LCMS_EXPANDING_ALIAS_COMMON_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/cmyk.h"
#include "tests/data/color-numeric/gray-samples.h"
#include "tests/data/color-numeric/lcms-expanding-alias.h"

static void
lcms_expand_fill(float *buffer, int kind, size_t count)
{
    unsigned char *bytes;
    uint16_t *words;
    int format;
    size_t channels;
    size_t pixel;
    size_t channel;
    size_t sample;
    float value;

    format = ea_cases[kind].format;
    channels = format == SIXEL_CMS_PIXELFORMAT_RGB_8 ? 3u
        : (format == SIXEL_CMS_PIXELFORMAT_GRAY_8 ||
           format == SIXEL_CMS_PIXELFORMAT_GRAY_F32) ? 1u : 4u;
    bytes = (unsigned char *)(buffer + 4);
    words = (uint16_t *)(buffer + 4);
    for (pixel = 0u; pixel < count; ++pixel) {
        sample = (pixel + pixel / 256u * 37u) % ea_cases[kind].count;
        for (channel = 0u; channel < channels; ++channel) {
            value = ea_cases[kind].source[sample][channel];
            if (format == SIXEL_CMS_PIXELFORMAT_CMYK_16) {
                words[pixel * channels + channel] = (uint16_t)value;
            } else if (format == SIXEL_CMS_PIXELFORMAT_GRAY_F32) {
                buffer[4u + pixel] = value;
            } else {
                bytes[pixel * channels + channel] = (unsigned char)value;
            }
        }
    }
}

static int
lcms_expand_alias(int argc, char **argv, int kind, int gamma)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    unsigned char const *profile;
    size_t profile_size;
    float *input;
    float original[521 * 3 + 8];
    float output[521 * 3 + 8];
    double const (*bounds)[3];
    size_t const counts[3] = { 1u, 2u, 521u };
    size_t pixel;
    size_t channel;
    size_t sample;
    size_t i;
    int format;
    int converted;
    int separate_only;
    int ok;

    if (argc != 1 && (argc != 2 || strcmp(argv[1], "separate") != 0)) {
        return 1;
    }
    separate_only = argc == 2;
    /* Allocated storage has no declared type: each input format can use
     * its own scalar type without aliasing a declared float array. */
    input = malloc(sizeof(original));
    if (input == NULL) {
        return 1;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_LCMS2);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_LCMS2) {
        sixel_cms_set_engine(previous);
        free(input);
        return 77;
    }
    format = ea_cases[kind].format;
    profile = color_icc;
    profile_size = sizeof(color_icc);
    if (format == SIXEL_CMS_PIXELFORMAT_GRAY_8 ||
        format == SIXEL_CMS_PIXELFORMAT_GRAY_F32) {
        profile = color_gray_icc;
        profile_size = sizeof(color_gray_icc);
    } else if (format == SIXEL_CMS_PIXELFORMAT_CMYK_8 ||
               format == SIXEL_CMS_PIXELFORMAT_CMYK_16) {
        profile = color_cmyk_icc;
        profile_size = sizeof(color_cmyk_icc);
    }
    memset(input, 0xa5, sizeof(original));
    memset(output, 0xa5, sizeof(output));
    lcms_expand_fill(input, kind, 521u);
    memcpy(original, input, sizeof(original));
    source = sixel_cms_open_profile_from_mem(profile, profile_size);
    target = gamma ? sixel_cms_create_srgb_profile()
                   : sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source,
        (sixel_cms_pixel_format_t)format, target,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = sixel_cms_transform_engine(transform) == SIXEL_CMS_ENGINE_LCMS2 &&
        sixel_cms_do_transform(transform, input + 4, output + 4, 521u);
    for (pixel = 0u; ok && pixel < 521u; ++pixel) {
        sample = (pixel + pixel / 256u * 37u) % ea_cases[kind].count;
        bounds = ea_cases[kind].bounds[sample][gamma];
        for (channel = 0u; channel < 3u; ++channel) {
            if (!isfinite(output[4u + pixel * 3u + channel]) ||
                output[4u + pixel * 3u + channel] < bounds[0][channel] ||
                output[4u + pixel * 3u + channel] > bounds[1][channel]) {
                fprintf(stderr, "LCMS separate src=%d pixel=%zu channel=%zu "
                    "value=%.9g interval=[%.17g,%.17g]\n", format,
                    pixel, channel, output[4u + pixel * 3u + channel],
                    bounds[0][channel], bounds[1][channel]);
                ok = 0;
            }
        }
    }
    /* A blanket transform failure must not satisfy the alias contract. */
    if (memcmp(input, original, sizeof(original)) != 0) {
        fprintf(stderr, "LCMS separate source modified\n");
        ok = 0;
    }
    for (i = 0u; i < 4u * sizeof(float); ++i) {
        if (((unsigned char *)output)[i] != 0xa5 ||
            ((unsigned char *)(output + 4u + 521u * 3u))[i] != 0xa5) {
            fprintf(stderr, "LCMS separate output guard modified\n");
            ok = 0;
        }
    }
    if (ok) {
        fprintf(stderr, "LCMSALIAS|src=%d|gamma=%d|count=521|"
            "separate-values-verified\n", format, gamma);
    }
    if (ok && !separate_only) {
        for (i = 0u; i < 3u; ++i) {
            /* The arena fits the larger RGB result and is float-aligned.
             * Rejection must preserve it entirely, including input and
             * otherwise unused output space, even for a single pixel. */
            memset(input, 0xa5, sizeof(original));
            lcms_expand_fill(input, kind, counts[i]);
            memcpy(original, input, sizeof(original));
            converted = sixel_cms_do_transform(transform, input + 4,
                input + 4, counts[i]);
            if (converted ||
                memcmp(input, original, sizeof(original)) != 0) {
                fprintf(stderr, "LCMS alias src=%d count=%zu status=%d "
                    "was not rejected before all writes\n", format,
                    counts[i], converted);
                ok = 0;
            }
        }
        if (ok) {
            fprintf(stderr, "LCMSALIAS|src=%d|gamma=%d|counts=1,2,521|"
                "reject-bytes-verified\n", format, gamma);
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    free(input);
    return ok ? 0 : 1;
}
#endif
