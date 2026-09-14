/* SPDX-License-Identifier: MIT */
/* Builtin float normalization preserves original ICC sample colors. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sixel.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/png-samples.h"

int
test_cms_0015(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *profile;
    float pixels[8 * 3];
    double peak;
    double expected;
    int depth;
    int pixel;
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_BUILTIN) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    profile = sixel_cms_open_profile_from_mem(color_icc, sizeof(color_icc));
    ok = profile != NULL;
    for (depth = 0; ok && depth < 2; ++depth) {
        peak = depth == 0 ? 255.0 : 65535.0;
        for (pixel = 0; pixel < 8; ++pixel) {
            for (channel = 0; channel < 3; ++channel) {
                pixels[pixel * 3 + channel] = (float)(
                    color_png_source[depth][pixel][channel] / peak);
            }
        }
        ok = sixel_cms_convert_profile_to_linearrgb((unsigned char *)pixels,
            8, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
        for (pixel = 0; ok && pixel < 8; ++pixel) {
            for (channel = 0; channel < 3; ++channel) {
                expected = color_png_icc_linear[depth][pixel][channel];
                /* Include input and output float32 rounding, without
                 * allowing gamut clipping. */
                if (!isfinite(pixels[pixel * 3 + channel]) ||
                    fabs(pixels[pixel * 3 + channel] - expected) > 0.0000005) {
                    fprintf(stderr, "depth=%d pixel=%d channel=%d: "
                        "%.9g != %.9g\n", depth, pixel, channel,
                        pixels[pixel * 3 + channel], expected);
                    ok = 0;
                }
            }
        }
    }
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
