/* SPDX-License-Identifier: MIT */
/* A rejected numeric conversion must not publish partially converted data. */
#include <math.h>
#include <stdlib.h>
#include "src/colorspace.h"

int test_color_0012(int argc, char **argv)
{
    double out[3] = {7.0, 8.0, 9.0};
    double input[3] = {NAN, 0.2, 0.3};

    (void)argc;
    (void)argv;
    if (sixel_color_convert(out, input, SIXEL_COLORSPACE_LINEAR,
                            SIXEL_COLORSPACE_OKLAB)) {
        return EXIT_FAILURE;
    }
    input[0] = 0.1;
    if (sixel_color_convert(out, input, SIXEL_COLORSPACE_LINEAR, -1)) {
        return EXIT_FAILURE;
    }
    input[0] = -1.0;
    input[1] = -1.0;
    input[2] = -1.0;
    if (sixel_color_convert(out, input, SIXEL_COLORSPACE_LINEAR,
                            SIXEL_COLORSPACE_DIN99D)) {
        return EXIT_FAILURE;
    }
    return out[0] == 7.0 && out[1] == 8.0 && out[2] == 9.0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
