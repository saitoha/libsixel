/*
 * SPDX-License-Identifier: MIT
 * Invert independent DIN99d D65 vectors, without deriving them through
 * the forward converter under test. The blue vector exposes missing X'.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <sixel.h>

int
test_color_0002_din99d_inverse(int argc, char **argv)
{
    static float const expected[] = {
        0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f,
        0.214041140f, 0.214041140f, 0.214041140f
    };
    float pixels[] = {
        0.357716271f, 0.112762849f, -0.457907312f,
        0.570283185f, 0.395572188f, 0.255974559f,
        0.571738633f, 0.0f, 0.0f
    };
    SIXELSTATUS status;
    size_t i;

    (void)argc;
    (void)argv;
    status = sixel_helper_convert_colorspace((unsigned char *)pixels,
        sizeof(pixels), SIXEL_PIXELFORMAT_DIN99DFLOAT32,
        SIXEL_COLORSPACE_DIN99D, SIXEL_COLORSPACE_LINEAR);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); ++i) {
        if (!isfinite(pixels[i]) || fabs(pixels[i] - expected[i]) > 0.0007) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
