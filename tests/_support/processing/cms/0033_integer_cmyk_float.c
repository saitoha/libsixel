/* SPDX-License-Identifier: MIT */
/* Original CMYK integer channels enter a colorimetric LUT transform. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/cmyk.h"

int
test_cms_0033(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_engine_t requested;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *linear;
    sixel_cms_transform_t *transform;
    uint16_t original[521 * 4];
    float output[521 * 3 + 8];
    unsigned char *input8;
    uint16_t *input16;
    double const (*samples)[4];
    double const (*expected)[3];
    double tolerance;
    int depth;
    int alias;
    int knee;
    int pixel;
    int channel;
    int ok;

    if ((argc != 4 && argc != 5) ||
        !sixel_cms_engine_from_string(argv[1], &requested)) {
        return EXIT_FAILURE;
    }
    depth = atoi(argv[2]);
    alias = strcmp(argv[3], "alias") == 0;
    knee = argc == 5 && strcmp(argv[4], "knee") == 0;
    if (depth != 8 && depth != 16) {
        return EXIT_FAILURE;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(requested);
    if (sixel_cms_get_engine() != requested) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    source = sixel_cms_open_profile_from_mem(knee
        ? color_cmyk_knee_icc : color_cmyk_icc, knee
        ? sizeof(color_cmyk_knee_icc) : sizeof(color_cmyk_icc));
    linear = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source, depth == 8
        ? SIXEL_CMS_PIXELFORMAT_CMYK_8 : SIXEL_CMS_PIXELFORMAT_CMYK_16,
        linear, SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    samples = depth == 8 ? color_cmyk_source : color_cmyk_source16;
    expected = depth == 8 ? color_cmyk_linear : color_cmyk_linear16;
    if (knee) {
        expected = color_cmyk_knee_linear16;
    }
    for (pixel = 0; pixel < 521 * 3 + 8; ++pixel) {
        output[pixel] = -99.0f;
    }
    input16 = alias ? (uint16_t *)(output + 4) : original;
    input8 = (unsigned char *)input16;
    for (pixel = 0; pixel < 521; ++pixel) {
        for (channel = 0; channel < 4; ++channel) {
            if (depth == 8) {
                input8[pixel * 4 + channel] = (unsigned char)
                    samples[pixel % 8][channel];
            } else {
                input16[pixel * 4 + channel] = (uint16_t)
                    samples[pixel % 8][channel];
            }
        }
    }
    /* The TAP caller selects relative! explicitly. A2B1 owns these physical
     * XYZ values; perceptual black-point adjustments are a separate model. */
    ok = sixel_cms_transform_engine(transform) == requested &&
        sixel_cms_do_transform(transform, input8, output + 4, 521u);
    for (pixel = 0; ok && pixel < 521; ++pixel) {
        for (channel = 0; channel < 3; ++channel) {
            tolerance = requested == SIXEL_CMS_ENGINE_COLORSYNC
                ? .0001 : .0000005;
            /* Only interpolated LUT samples incur a fixed-point PCS
             * budget. Vertices and builtin float PCS keep the tight gate. */
            if (requested != SIXEL_CMS_ENGINE_BUILTIN &&
                (pixel % 8 == 5 || pixel % 8 == 6)) {
                tolerance += color_cmyk_lut_bound[channel];
            }
            if (!isfinite(output[4 + pixel * 3 + channel]) ||
                fabs(output[4 + pixel * 3 + channel] -
                    expected[pixel % 8][channel]) > tolerance) {
                fprintf(stderr, "depth=%d pixel=%d channel=%d %.9g != %.9g\n",
                    depth, pixel, channel, output[4 + pixel * 3 + channel],
                    expected[pixel % 8][channel]);
                ok = 0;
            }
        }
        for (channel = 0; !alias && channel < 4; ++channel) {
            if ((depth == 8 ? input8[pixel * 4 + channel]
                           : input16[pixel * 4 + channel]) !=
                samples[pixel % 8][channel]) {
                ok = 0;
            }
        }
    }
    for (pixel = 0; pixel < 4; ++pixel) {
        if (output[pixel] != -99.0f || output[4 + 521 * 3 + pixel] != -99.0f) {
            ok = 0;
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(linear);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
