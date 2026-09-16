/* SPDX-License-Identifier: MIT */
/* Independent matched values precede known-profile rejection assertions. */
#ifndef TEST_PROFILE_FORMAT_COMMON_H
#define TEST_PROFILE_FORMAT_COMMON_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/profile-format.h"

static sixel_cms_profile_t *
profile_format_open(int profile)
{
    if (profile == -1) {
        return sixel_cms_create_linear_srgb_profile();
    }
    if (profile == -2) {
        return sixel_cms_create_srgb_profile();
    }
    if (profile == -3) {
        return sixel_cms_create_cielab_d50_profile();
    }
    return sixel_cms_open_profile_from_mem(pf_profiles[profile].bytes,
                                            pf_profiles[profile].size);
}

static int
profile_format_call(sixel_cms_transform_t *transform, size_t specimen,
                    size_t count, int alias, int reject)
{
    unsigned char before[521u * 16u + 128u];
    unsigned char out_before[sizeof(before)];
    unsigned char *input;
    unsigned char *output;
    unsigned char *destination;
    double const (*bounds)[3];
    double number;
    float value;
    uint16_t word;
    size_t pixel;
    size_t sample;
    size_t channel;
    size_t offset;
    size_t i;
    int format;
    int channels;
    int applied;
    int ok;

    /* Allocated storage permits the original byte/word/float scalar type.
     * The two small invalid counts fit all native scratch representations;
     * no known mismatched large native row is ever invoked by this test. */
    input = malloc(sizeof(before));
    output = malloc(sizeof(before));
    if (input == NULL || output == NULL) {
        free(input);
        free(output);
        return 0;
    }
    memset(input, 0xa5, sizeof(before));
    memset(output, 0xa5, sizeof(before));
    format = pf_cases[specimen].format;
    channels = pf_cases[specimen].channels;
    offset = 32u;
    for (pixel = 0u; pixel < count; ++pixel) {
        sample = (pixel + pixel / 256u * 37u) % 2u;
        for (channel = 0u; channel < (size_t)channels; ++channel) {
            number = pf_source[specimen][sample][channel];
            if (format == SIXEL_CMS_PIXELFORMAT_CMYK_16) {
                word = (uint16_t)number;
                memcpy(input + offset, &word, sizeof(word));
                offset += sizeof(word);
            } else if (format == SIXEL_CMS_PIXELFORMAT_RGB_F32 ||
                       format == SIXEL_CMS_PIXELFORMAT_CMYK_F32 ||
                       format == SIXEL_CMS_PIXELFORMAT_LAB_F32 ||
                       format == SIXEL_CMS_PIXELFORMAT_GRAY_F32) {
                value = (float)number;
                memcpy(input + offset, &value, sizeof(value));
                offset += sizeof(value);
            } else {
                input[offset++] = (unsigned char)number;
            }
        }
    }
    memcpy(before, input, sizeof(before));
    destination = alias ? input : output;
    memcpy(out_before, destination, sizeof(out_before));
    applied = transform == NULL ? 0 : sixel_cms_do_transform(transform,
        input + 32u, destination + 32u, count);
    if (reject) {
        ok = !applied && memcmp(input, before, sizeof(before)) == 0 &&
             memcmp(destination, out_before, sizeof(out_before)) == 0;
    } else {
        ok = applied && memcmp(input, before, sizeof(before)) == 0;
        for (pixel = 0u; ok && pixel < count; ++pixel) {
            sample = (pixel + pixel / 256u * 37u) % 2u;
            bounds = pf_bounds[specimen][sample];
            for (channel = 0u; channel < 3u; ++channel) {
                memcpy(&value, destination + 32u +
                    (pixel * 3u + channel) * sizeof(float), sizeof(value));
                if (!isfinite(value) || value < bounds[0][channel] ||
                    value > bounds[1][channel]) {
                    fprintf(stderr, "Profile positive case=%zu pixel=%zu "
                        "component=%zu value=%.9g interval=[%.17g,%.17g]\n",
                        specimen, pixel, channel, value,
                        bounds[0][channel], bounds[1][channel]);
                    ok = 0;
                }
            }
        }
        for (i = 0u; i < sizeof(before); ++i) {
            if ((i < 32u || i >= 32u + count * 3u * sizeof(float)) &&
                destination[i] != out_before[i]) {
                ok = 0;
            }
        }
    }
    if (!ok) {
        fprintf(stderr, "Profile domain case=%zu count=%zu alias=%d "
            "reject=%d applied=%d\n", specimen, count, alias,
            reject, applied);
    }
    free(input);
    free(output);
    return ok;
}

static int
profile_format_test(size_t specimen)
{
    int const profiles[7] = { -2, -1, 3, 1, 2, -3, 6 };
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    size_t i;
    size_t count;
    int engine;
    int format;
    int expected;
    int actual;
    int alias;
    int ok;

    previous = sixel_cms_get_engine();
    engine = pf_cases[specimen].engine;
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    format = pf_cases[specimen].format;
    expected = pf_cases[specimen].expected_domain;
    source = profile_format_open(pf_cases[specimen].profile);
    target = profile_format_open(pf_cases[specimen].target);
    transform = sixel_cms_create_transform(source, format, target,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    /* Native creation alone is not a numerical positive. Every component
     * must satisfy the original independently derived 521-pixel bounds. */
    fprintf(stderr, "PFTRACE|kind=positive|engine=%d|src=%d\n",
        engine, format);
    ok = source != NULL && target != NULL &&
        (int)sixel_cms_transform_engine(transform) == engine &&
        profile_format_call(transform, specimen, 521u, 0, 0);
    fprintf(stderr, "PFTRACE|kind=end\n");
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(source);
    for (i = 0u; ok && i < 7u; ++i) {
        source = profile_format_open(profiles[i]);
        if (source == NULL) {
            ok = 0;
            break;
        }
        actual = sixel_cms_get_color_space(source);
        if (actual == expected) {
            sixel_cms_close_profile(source);
            continue;
        }
        transform = sixel_cms_create_transform(source, format, target,
            SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
        ok = actual >= SIXEL_CMS_COLORSPACE_RGB &&
            actual <= SIXEL_CMS_COLORSPACE_LAB &&
            (transform == NULL ||
             (int)sixel_cms_transform_engine(transform) == engine);
        /* Backend creation rejection and shared apply rejection both
         * satisfy this behavioral contract. Trace ownership is separate. */
        for (count = 1u; ok && count <= 2u; ++count) {
            for (alias = 0; ok && alias < 2; ++alias) {
                fprintf(stderr, "PFTRACE|kind=known|engine=%d|src=%d|"
                    "profile=%d|expected=%d|created=%d\n", engine, format,
                    actual, expected, transform != NULL);
                ok = profile_format_call(transform, specimen, count,
                                           alias, 1);
                fprintf(stderr, "PFTRACE|kind=end\n");
            }
        }
        sixel_cms_delete_transform(transform);
        sixel_cms_close_profile(source);
    }
    sixel_cms_close_profile(target);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "PFDOMAIN|case=%zu|engine=%d|src=%d|gamma=%d|"
            "independent-positive-and-prewrite-rejection-verified\n",
            specimen, engine, format, pf_cases[specimen].gamma);
    }
    return ok ? 0 : 1;
}
#endif
