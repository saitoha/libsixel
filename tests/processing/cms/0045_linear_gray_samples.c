/* SPDX-License-Identifier: MIT */
/* Replicated original gray samples reach a compatible float transform. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/cms.h"
#include "tests/data/color-numeric/gray-samples.h"
#include "tests/data/color-numeric/gray-variants.h"

int
test_cms_0045(int argc, char **argv)
{
    sixel_cms_engine_t engine;
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *profile;
    float pixels[COLOR_GRAY_SAMPLE_COUNT * 3];
    double bound;
    int p;
    int c;
    int ok;
    int variant;
    double const *expected;

    if ((argc != 2 && argc != 3) ||
        !sixel_cms_engine_from_string(argv[1], &engine)) {
        return 1;
    }
    variant = argc == 3 ? atoi(argv[2]) : -1;
    if (variant < -1 || variant >= 6) {
        return 1;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    if (sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    profile = sixel_cms_open_profile_from_mem(
        variant < 0 ? color_gray_icc : color_gray_variants[variant].bytes,
        variant < 0 ? sizeof(color_gray_icc)
                    : color_gray_variants[variant].length);
    expected = variant < 0 ? color_gray_linear
                          : color_gray_variants[variant].linear;
    for (p = 0; p < COLOR_GRAY_SAMPLE_COUNT; ++p) {
        for (c = 0; c < 3; ++c) {
            pixels[p * 3 + c] = (float)(color_gray_source[p] / 65535.0);
        }
    }
    ok = profile != NULL && sixel_cms_convert_profile_to_linearrgb(
        (unsigned char *)pixels, COLOR_GRAY_SAMPLE_COUNT, 1,
        SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    /* Gray is Y times D50, so matrix and curve errors scale with Y.
     * The independent generator propagates fixed-point profile precision
     * and a binary32 arithmetic budget. There is no absolute dark floor;
     * expected black must be exactly zero, positive dark values survive. */
    bound = engine == SIXEL_CMS_ENGINE_COLORSYNC
        ? color_gray_native_relative : color_gray_float_relative;
    for (p = 0; ok && p < COLOR_GRAY_SAMPLE_COUNT; ++p) {
        for (c = 0; c < 3; ++c) {
            if (!isfinite(pixels[p * 3 + c]) ||
                fabs(pixels[p * 3 + c] - expected[p]) > expected[p] * bound) {
                fprintf(stderr, "gray sample=%d channel=%d %.10g != %.10g\n",
                    p, c, pixels[p * 3 + c], expected[p]);
                ok = 0;
            }
        }
    }
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
