/* SPDX-License-Identifier: MIT */
/* Keep valid UNKNOWN layouts and check the native channel rejection trace. */
#ifndef TEST_COLORSYNC_CHANNEL_SCOPE_COMMON_H
#define TEST_COLORSYNC_CHANNEL_SCOPE_COMMON_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/colorsync-channels.h"
#include "tests/data/color-numeric/colorsync-channel-scope.h"

static int
colorsync_channel_scope_call(sixel_cms_profile_t *source,
    sixel_cms_profile_t *target, int format, double const values[2][4],
    int components, int gamma, int xyz_case, int reject, size_t count,
    int alias)
{
    unsigned char before[521u * 16u + 128u];
    unsigned char output_before[sizeof(before)];
    unsigned char *input;
    unsigned char *output;
    unsigned char *destination;
    float *reference_source;
    float *reference_output;
    sixel_cms_transform_t *transform;
    sixel_cms_transform_t *reference;
    double const (*bounds)[3];
    double value;
    float stored;
    float converted;
    uint16_t word;
    size_t pixel;
    size_t sample;
    size_t channel;
    size_t offset;
    size_t i;
    int colorants;
    int ok;
    int applied;
    int nonzero;

    input = malloc(sizeof(before));
    output = malloc(sizeof(before));
    reference_source = malloc(sizeof(before));
    reference_output = malloc(sizeof(before));
    transform = NULL;
    reference = NULL;
    ok = 0;
    if (input == NULL || output == NULL || reference_source == NULL ||
        reference_output == NULL) {
        goto end;
    }
    memset(input, 0xa5, sizeof(before));
    memset(output, 0xa5, sizeof(before));
    colorants = components - (format == SIXEL_CMS_PIXELFORMAT_RGBA_8);
    offset = 32u;
    for (pixel = 0u; pixel < count; ++pixel) {
        sample = (pixel + pixel / 256u * 37u) % 2u;
        for (channel = 0u; channel < (size_t)components; ++channel) {
            value = values[sample][channel];
            if (format == SIXEL_CMS_PIXELFORMAT_CMYK_16) {
                word = (uint16_t)value;
                memcpy(input + offset, &word, sizeof(word));
                offset += sizeof(word);
                converted = (float)(word / 65535.0);
            } else if (format == SIXEL_CMS_PIXELFORMAT_RGB_F32 ||
                       format == SIXEL_CMS_PIXELFORMAT_LAB_F32) {
                stored = (float)value;
                memcpy(input + offset, &stored, sizeof(stored));
                offset += sizeof(stored);
                converted = format == SIXEL_CMS_PIXELFORMAT_LAB_F32 &&
                    channel != 0u ? (float)(stored * (128.0 / 255.0) + 0.5)
                                  : stored;
            } else {
                input[offset++] = (unsigned char)value;
                converted = (float)((unsigned char)value / 255.0);
            }
            if (channel < (size_t)colorants) {
                reference_source[pixel * (size_t)colorants + channel] =
                    converted;
            }
        }
    }
    memcpy(before, input, sizeof(before));
    destination = alias ? input : output;
    memcpy(output_before, destination, sizeof(output_before));
    transform = sixel_cms_create_transform(source, format, target,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    if (sixel_cms_transform_engine(transform) != SIXEL_CMS_ENGINE_COLORSYNC) {
        goto end;
    }
    applied = sixel_cms_do_transform(transform, input + 32u,
                                     destination + 32u, count);
    if (reject) {
        ok = !applied && memcmp(input, before, sizeof(before)) == 0 &&
             memcmp(destination, output_before, sizeof(output_before)) == 0;
        goto end;
    }
    /* These UNKNOWN profiles retain their native homogeneous float path.
     * Compare the adapter with explicitly prepared native units, and also
     * check independent XYZ intervals so an all-zero result cannot pass. */
    reference = sixel_cms_create_transform(source, colorants == 4
        ? SIXEL_CMS_PIXELFORMAT_CMYK_F32 : SIXEL_CMS_PIXELFORMAT_RGB_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = applied && sixel_cms_transform_engine(reference) ==
         SIXEL_CMS_ENGINE_COLORSYNC && sixel_cms_do_transform(reference,
            reference_source, reference_output, count);
    ok = ok && memcmp(destination + 32u, reference_output,
                       count * 3u * sizeof(float)) == 0;
    if (!alias && memcmp(input, before, sizeof(before)) != 0) {
        ok = 0;
    }
    nonzero = 0;
    for (pixel = 0u; ok && pixel < count; ++pixel) {
        sample = (pixel + pixel / 256u * 37u) % 2u;
        for (channel = 0u; channel < 3u; ++channel) {
            stored = reference_output[pixel * 3u + channel];
            if (!isfinite(stored)) {
                ok = 0;
            }
            nonzero |= stored != 0.0f;
            if (xyz_case >= 0) {
                bounds = sc_xyz_bounds[xyz_case][sample][gamma];
                if (stored < bounds[0][channel] ||
                    stored > bounds[1][channel]) {
                    ok = 0;
                }
            }
        }
    }
    ok = ok && nonzero;
    for (i = 0u; i < sizeof(before); ++i) {
        if ((i < 32u || i >= 32u + count * 3u * sizeof(float)) &&
            destination[i] != output_before[i]) {
            ok = 0;
        }
    }
end:
    if (!ok) {
        fprintf(stderr, "Channel scope fmt=%d gamma=%d count=%zu "
            "alias=%d reject=%d xyz=%d\n", format, gamma, count, alias,
            reject, xyz_case);
    }
    sixel_cms_delete_transform(reference);
    sixel_cms_delete_transform(transform);
    free(input);
    free(output);
    free(reference_source);
    free(reference_output);
    return ok;
}

static int
colorsync_channel_scope(int gamma)
{
    size_t const counts[6] = { 1u, 2u, 255u, 256u, 257u, 521u };
    int const xyz_formats[3] = { 2, 3, 4 };
    size_t const multichannel[5] = { 1u, 2u, 5u, 3u, 4u };
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    unsigned char const *bytes;
    size_t length;
    size_t test;
    size_t kind;
    size_t specimen;
    int components;
    int colorants;
    int alias;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    target = sixel_cms_open_profile_from_mem(gamma ? sc_gamma_icc
        : sc_linear_icc, gamma ? sizeof(sc_gamma_icc) : sizeof(sc_linear_icc));
    source = sixel_cms_open_profile_from_mem(sc_xyz_icc, sizeof(sc_xyz_icc));
    ok = target != NULL && source != NULL &&
         sixel_cms_get_color_space(source) == SIXEL_CMS_COLORSPACE_UNKNOWN;
    for (kind = 0u; ok && kind < 3u; ++kind) {
        components = xyz_formats[kind] == SIXEL_CMS_PIXELFORMAT_RGBA_8
            ? 4 : 3;
        for (test = 0u; ok && test < 6u; ++test) {
            for (alias = 0; ok && alias < 2; ++alias) {
                ok = colorsync_channel_scope_call(source, target,
                    xyz_formats[kind], sc_xyz_source[kind], components,
                    gamma, (int)kind, 0, counts[test], alias);
            }
        }
    }
    sixel_cms_close_profile(source);
    for (kind = 0u; ok && kind < 5u; ++kind) {
        specimen = multichannel[kind];
        components = sc_cases[specimen].channels;
        bytes = kind < 3u ? sc_clr3_icc : sc_clr4_icc;
        length = kind < 3u ? sizeof(sc_clr3_icc) : sizeof(sc_clr4_icc);
        source = sixel_cms_open_profile_from_mem(bytes, length);
        ok = source != NULL && sixel_cms_get_color_space(source) ==
             SIXEL_CMS_COLORSPACE_UNKNOWN;
        for (test = 0u; ok && test < 6u; ++test) {
            for (alias = 0; ok && alias < 2; ++alias) {
                ok = colorsync_channel_scope_call(source, target,
                    sc_cases[specimen].format, sc_cases[specimen].source,
                    components, gamma, -1, 0, counts[test], alias);
            }
        }
        sixel_cms_close_profile(source);
    }
    /* UNKNOWN sources keep these exact trace assertions owned by the
     * native-layout guard when a separate known-domain guard is added. */
    for (specimen = 0u; ok && specimen < 6u; ++specimen) {
        components = sc_cases[specimen].channels;
        colorants = components -
            (sc_cases[specimen].format == SIXEL_CMS_PIXELFORMAT_RGBA_8);
        bytes = colorants == 3 ? sc_clr4_icc : sc_xyz_icc;
        length = colorants == 3 ? sizeof(sc_clr4_icc) : sizeof(sc_xyz_icc);
        source = sixel_cms_open_profile_from_mem(bytes, length);
        ok = source != NULL && sixel_cms_get_color_space(source) ==
             SIXEL_CMS_COLORSPACE_UNKNOWN;
        for (alias = 0; ok && alias < 2; ++alias) {
            ok = colorsync_channel_scope_call(source, target,
                sc_cases[specimen].format, sc_cases[specimen].source,
                components, gamma, -1, 1, 2u, alias);
        }
        sixel_cms_close_profile(source);
    }
    sixel_cms_close_profile(target);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "CSCHANNELSCOPE|gamma=%d|"
            "xyz-values-native-units-and-guards-verified\n", gamma);
    }
    return ok ? 0 : 1;
}
#endif
