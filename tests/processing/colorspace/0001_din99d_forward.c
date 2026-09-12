/*
 * SPDX-License-Identifier: MIT
 * Verify independently calculated DIN99d D65 reference coordinates.
 * Cui et al. (2002), DOI 10.1002/col.10066, equation (5), with the
 * sRGB matrix/reference white in src/colorspace.c and one common /100 scale.
 * These expectations include the X' correction, including reference white.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <sixel.h>

int
test_color_0001_din99d_forward(int argc, char **argv)
{
    static float const expected[] = {
        0.357716271f, 0.112762849f, -0.457907312f,
        0.570283185f, 0.395572188f, 0.255974559f,
        0.571738633f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    float pixels[] = {
        0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f,
        0.5f, 0.5f, 0.5f,
        1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f
    };
    SIXELSTATUS status;
    size_t i;

    (void)argc;
    (void)argv;
    status = sixel_helper_convert_colorspace((unsigned char *)pixels,
        sizeof(pixels), SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_DIN99D);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    /* Allow the converter's cube-root/transfer lookup approximation. */
    for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); ++i) {
        if (!isfinite(pixels[i]) || fabs(pixels[i] - expected[i]) > 0.0007) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
