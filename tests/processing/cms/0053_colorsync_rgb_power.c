/* SPDX-License-Identifier: MIT */
/* Original binary32 samples follow the independently stored ICC definition. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/rgb-power.h"

int
test_cms_0053(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_engine_t engine;
    sixel_cms_engine_t selected;
    sixel_cms_profile_t *profile;
    float pixels[COLOR_RGB_POWER_SAMPLES * 3];
    double scale;
    double expected;
    int variant;
    int strict;
    int p;
    int c;
    int ok;

    if (argc != 4 ||
        !sixel_cms_engine_from_string(argv[3], &engine)) {
        return 1;
    }
    variant = atoi(argv[1]);
    strict = strcmp(argv[2], "strict") == 0;
    if (variant < 0 || variant >= COLOR_RGB_POWER_PROFILES) {
        return 1;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    profile = NULL;
    if (!strict) {
        profile = sixel_cms_open_profile_from_mem(
            color_rgb_power_profiles[variant].bytes,
            color_rgb_power_profiles[variant].length);
    }
    sixel_cms_set_engine(engine);
    selected = sixel_cms_get_engine();
    for (p = 0; p < COLOR_RGB_POWER_SAMPLES; ++p) {
        for (c = 0; c < 3; ++c) {
            pixels[p * 3 + c] =
                (float)(color_rgb_power_source[p][c] / 65535.0);
        }
    }
    ok = strict ? sixel_cms_convert_colorsync_linear(pixels,
        COLOR_RGB_POWER_SAMPLES, 1, 0,
        color_rgb_power_profiles[variant].bytes,
        color_rgb_power_profiles[variant].length)
        : profile != NULL && sixel_cms_convert_profile_to_linearrgb(
            (unsigned char *)pixels, COLOR_RGB_POWER_SAMPLES, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    ok = ok && sixel_cms_get_engine() == selected;
    /* Scale each triplet's error by its expected vector magnitude. This
     * permits fixed-point matrix cross-talk near a primary's zero channel,
     * but provides no absolute floor that could hide dark-sample loss.
     * In particular, black must remain exactly zero in all three channels. */
    for (p = 0; ok && p < COLOR_RGB_POWER_SAMPLES; ++p) {
        scale = 0.0;
        for (c = 0; c < 3; ++c) {
            expected = color_rgb_power_profiles[variant].linear[p][c];
            if (fabs(expected) > scale) {
                scale = fabs(expected);
            }
        }
        for (c = 0; c < 3; ++c) {
            expected = color_rgb_power_profiles[variant].linear[p][c];
            if (!isfinite(pixels[p * 3 + c]) ||
                fabs(pixels[p * 3 + c] - expected) >
                    scale * color_rgb_power_relative) {
                fprintf(stderr, "%s sample=%d channel=%d %.17g != %.17g\n",
                    color_rgb_power_profiles[variant].name, p, c,
                    pixels[p * 3 + c], expected);
                ok = 0;
            }
        }
    }
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
