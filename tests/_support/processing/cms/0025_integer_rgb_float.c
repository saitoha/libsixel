/* SPDX-License-Identifier: MIT */
/* Original interleaved RGB/RGBA bytes, including alpha, enter the CMS API. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/png-samples.h"

int
test_cms_0025(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_engine_t requested;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *linear;
    sixel_cms_transform_t *transform;
    unsigned char original[521 * 4 + 8];
    float output[521 * 3 + 8];
    unsigned char *input;
    double tolerance;
    double expected;
    int channels;
    int alias;
    int pixel;
    int channel;
    int ok;

    if (argc != 4 || !sixel_cms_engine_from_string(argv[1], &requested)) {
        return EXIT_FAILURE;
    }
    channels = atoi(argv[2]);
    alias = strcmp(argv[3], "alias") == 0;
    if (channels != 3 && channels != 4) {
        return EXIT_FAILURE;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(requested);
    if (sixel_cms_get_engine() != requested) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    source = sixel_cms_open_profile_from_mem(color_icc, sizeof(color_icc));
    linear = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source, channels == 3
        ? SIXEL_CMS_PIXELFORMAT_RGB_8 : SIXEL_CMS_PIXELFORMAT_RGBA_8,
        linear, SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    memset(original, 0xa5, sizeof(original));
    for (pixel = 0; pixel < 521 * 3 + 8; ++pixel) {
        output[pixel] = -99.0f;
    }
    input = alias ? (unsigned char *)(output + 4) : original + 4;
    for (pixel = 0; pixel < 521; ++pixel) {
        for (channel = 0; channel < channels; ++channel) {
            input[pixel * channels + channel] = (unsigned char)
                color_png_source[0][pixel % 8][channel];
        }
    }
    ok = sixel_cms_transform_engine(transform) == requested &&
        sixel_cms_do_transform(transform, input, output + 4, 521u);
    /* Retain the existing P3 profile precision bound for ColorSync. Other
     * backends need only float rounding against the encoded ICC oracle. */
    tolerance = requested == SIXEL_CMS_ENGINE_COLORSYNC ? .0001 : .0000005;
    for (pixel = 0; ok && pixel < 521; ++pixel) {
        for (channel = 0; channel < 3; ++channel) {
            expected = color_png_icc_linear[0][pixel % 8][channel];
            if (!isfinite(output[4 + pixel * 3 + channel]) ||
                fabs(output[4 + pixel * 3 + channel] - expected) > tolerance) {
                fprintf(stderr, "pixel=%d channel=%d %.9g != %.9g\n",
                    pixel, channel, output[4 + pixel * 3 + channel], expected);
                ok = 0;
            }
        }
        for (channel = 0; !alias && channel < channels; ++channel) {
            if (input[pixel * channels + channel] !=
                color_png_source[0][pixel % 8][channel]) {
                ok = 0;
            }
        }
    }
    for (pixel = 0; pixel < 4; ++pixel) {
        if (output[pixel] != -99.0f || output[4 + 521 * 3 + pixel] != -99.0f ||
            original[pixel] != 0xa5 ||
            original[4 + 521 * channels + pixel] != 0xa5) {
            ok = 0;
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(linear);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
