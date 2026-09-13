/* SPDX-License-Identifier: MIT */
/* Independent coordinates include negative LMS and values above the LUT. */
#include <math.h>
#include <stdlib.h>
#include "src/colorspace.h"
#include "tests/data/color-numeric/opponents.h"

int test_color_0011(int argc, char **argv)
{
    double actual[3];
    size_t i;
    int channel;

    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(color_opponents) / sizeof(color_opponents[0]);
         ++i) {
        if (!sixel_color_convert(actual, color_opponents[i] + 9,
                SIXEL_COLORSPACE_DIN99D, SIXEL_COLORSPACE_LINEAR)) {
            return EXIT_FAILURE;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(actual[channel]) || fabs(actual[channel] -
                    color_opponents[i][0 + channel]) > 1e-6) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
