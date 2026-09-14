/* SPDX-License-Identifier: MIT */
/* The in-place transfer helper preserves extended finite RGB. */
#include <math.h>
#include <stdlib.h>
#include "src/cms.h"
#include "tests/data/color-numeric/trc.h"

int
test_cms_0018(int argc, char **argv)
{
    float pixels[3];
    size_t i;
    size_t channel;

    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(color_trc) / sizeof(color_trc[0]); ++i) {
        for (channel = 0; channel < 3; ++channel) {
            pixels[channel] = (float)color_trc[i][0];
        }
        if (!sixel_cms_convert_rgbf32_gamma_to_linear(pixels, 1u)) {
            return EXIT_FAILURE;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(pixels[channel]) ||
                fabs(pixels[channel] - color_trc[i][1]) > 0.0000005) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
