/* SPDX-License-Identifier: MIT */
/* Original byte samples reach the same extended linear RGB as float input. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/png-samples.h"

int
test_cms_0017(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *linear;
    sixel_cms_transform_t *transform;
    unsigned char pixels[8 * 3];
    float actual[8 * 3];
    int pixel;
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = sixel_cms_open_profile_from_mem(color_icc, sizeof(color_icc));
    linear = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source,
        SIXEL_CMS_PIXELFORMAT_RGB_8, linear, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        SIXEL_CMS_TRANSFORM_DEFAULT);
    for (pixel = 0; pixel < 8; ++pixel) {
        for (channel = 0; channel < 3; ++channel) {
            pixels[pixel * 3 + channel] = (unsigned char)
                color_png_source[0][pixel][channel];
        }
    }
    ok = sixel_cms_do_transform(transform, pixels, actual, 8u);
    for (pixel = 0; ok && pixel < 8; ++pixel) {
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(actual[pixel * 3 + channel]) ||
                fabs(actual[pixel * 3 + channel] -
                    color_png_icc_linear[0][pixel][channel]) > 0.0000005) {
                fprintf(stderr, "pixel=%d channel=%d: %.9g\n", pixel,
                    channel, actual[pixel * 3 + channel]);
                ok = 0;
            }
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(source);
    sixel_cms_close_profile(linear);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
