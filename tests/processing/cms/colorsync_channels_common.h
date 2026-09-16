/* SPDX-License-Identifier: MIT */
/* Independent positive values and prewrite native channel-layout rejection. */
#ifndef TEST_COLORSYNC_CHANNELS_COMMON_H
#define TEST_COLORSYNC_CHANNELS_COMMON_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/colorsync-channels.h"

static int
colorsync_channel_call(sixel_cms_transform_t *transform, size_t specimen,
                       int gamma, size_t count, int alias, int reject)
{
    unsigned char original[521u * 16u + 128u];
    unsigned char output_before[sizeof(original)];
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

    /* Allocated storage has no declared scalar type. Typed samples are
     * copied into aligned addresses without aliasing a declared array. */
    input = malloc(sizeof(original));
    output = malloc(sizeof(original));
    if (input == NULL || output == NULL) {
        free(input);
        free(output);
        return 0;
    }
    memset(input, 0xa5, sizeof(original));
    memset(output, 0xa5, sizeof(original));
    format = sc_cases[specimen].format;
    channels = sc_cases[specimen].channels;
    offset = 32u;
    for (pixel = 0u; pixel < count; ++pixel) {
        sample = (pixel + pixel / 256u * 37u) % 2u;
        for (channel = 0u; channel < (size_t)channels; ++channel) {
            number = sc_cases[specimen].source[sample][channel];
            if (format == SIXEL_CMS_PIXELFORMAT_CMYK_16) {
                word = (uint16_t)number;
                memcpy(input + offset, &word, sizeof(word));
                offset += sizeof(word);
            } else if (format == SIXEL_CMS_PIXELFORMAT_LAB_F32) {
                value = (float)number;
                memcpy(input + offset, &value, sizeof(value));
                offset += sizeof(value);
            } else {
                input[offset++] = (unsigned char)number;
            }
        }
    }
    memcpy(original, input, sizeof(original));
    destination = alias ? input : output;
    memcpy(output_before, destination, sizeof(output_before));
    applied = sixel_cms_do_transform(transform, input + 32u,
                                     destination + 32u, count);
    if (reject) {
        ok = !applied && memcmp(input, original, sizeof(original)) == 0 &&
             memcmp(destination, output_before, sizeof(output_before)) == 0;
    } else {
        ok = applied && (alias ||
             memcmp(input, original, sizeof(original)) == 0);
        for (pixel = 0u; ok && pixel < count; ++pixel) {
            sample = (pixel + pixel / 256u * 37u) % 2u;
            bounds = sc_cases[specimen].bounds[sample][gamma];
            for (channel = 0u; channel < 3u; ++channel) {
                memcpy(&value, destination + 32u +
                    (pixel * 3u + channel) * sizeof(float), sizeof(value));
                if (!isfinite(value) || value < bounds[0][channel] ||
                    value > bounds[1][channel]) {
                    fprintf(stderr, "Channel positive fmt=%d pixel=%zu "
                        "component=%zu value=%.9g interval=[%.9g,%.9g]\n",
                        format, pixel, channel, value, bounds[0][channel],
                        bounds[1][channel]);
                    ok = 0;
                }
            }
        }
        for (i = 0u; i < sizeof(original); ++i) {
            if ((i < 32u || i >= 32u + count * 3u * sizeof(float)) &&
                destination[i] != output_before[i]) {
                ok = 0;
            }
        }
    }
    if (!ok) {
        fprintf(stderr, "Channel contract fmt=%d gamma=%d count=%zu "
            "alias=%d reject=%d applied=%d\n", format, gamma, count,
            alias, reject, applied);
    }
    free(input);
    free(output);
    return ok;
}

static int
colorsync_channels(size_t specimen, int gamma)
{
    size_t const counts[6] = { 1u, 2u, 255u, 256u, 257u, 521u };
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    size_t test;
    size_t profile;
    size_t rejected;
    int format;
    int colorants;
    int alias;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    format = sc_cases[specimen].format;
    colorants = sc_cases[specimen].channels -
        (format == SIXEL_CMS_PIXELFORMAT_RGBA_8 ? 1 : 0);
    if (format == SIXEL_CMS_PIXELFORMAT_LAB_F32) {
        target = sixel_cms_open_profile_from_mem(gamma ? sc_gamma_icc
            : sc_linear_icc, gamma ? sizeof(sc_gamma_icc)
                                  : sizeof(sc_linear_icc));
    } else {
        target = gamma ? sixel_cms_create_srgb_profile()
                       : sixel_cms_create_linear_srgb_profile();
    }
    source = sixel_cms_open_profile_from_mem(sc_cases[specimen].profile,
                                             sc_cases[specimen].size);
    transform = sixel_cms_create_transform(source, format, target,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = sixel_cms_transform_engine(transform) == SIXEL_CMS_ENGINE_COLORSYNC;
    /* Successful creation alone cannot validate a numerical positive. */
    for (test = 0u; ok && test < 6u; ++test) {
        for (alias = 0; ok && alias < 2; ++alias) {
            ok = colorsync_channel_call(transform, specimen, gamma,
                                          counts[test], alias, 0);
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(source);
    rejected = 0u;
    /* Run every one- and two-pixel mismatch before any larger mismatch.
     * The old implementation and guard-bypass mutants must fail here and
     * stop: a CMYK transform cannot safely read a full three-channel Lab
     * scratch batch. A failed rejection is never followed by a large call. */
    for (test = 0u; ok && test < 6u; ++test) {
        for (profile = 0u; ok && profile <
                sizeof(sc_profiles) / sizeof(sc_profiles[0]); ++profile) {
            if (sc_profiles[profile].colorants == colorants) {
                continue;
            }
            source = sixel_cms_open_profile_from_mem(
                sc_profiles[profile].bytes, sc_profiles[profile].size);
            transform = sixel_cms_create_transform(source, format, target,
                SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
            ok = sixel_cms_transform_engine(transform) ==
                 SIXEL_CMS_ENGINE_COLORSYNC;
            for (alias = 0; ok && alias < 2; ++alias) {
                ok = colorsync_channel_call(transform, specimen, gamma,
                                              counts[test], alias, 1);
                rejected += ok != 0;
            }
            sixel_cms_delete_transform(transform);
            sixel_cms_close_profile(source);
        }
    }
    sixel_cms_close_profile(target);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "CSCHANNEL|format=%d|gamma=%d|rejected=%zu|"
            "independent-values-and-prewrite-guards-verified\n",
            format, gamma, rejected);
    }
    return ok ? 0 : 1;
}
#endif
