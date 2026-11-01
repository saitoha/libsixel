/* SPDX-License-Identifier: MIT */
/* Independent positive/negative TRC knees and extended range. */
#include <math.h>
#include <stdlib.h>
#include "src/colorspace.h"
#include "tests/data/color-numeric/trc.h"

int test_color_0005(int argc, char **argv)
{
    double actual;
    size_t i;

    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(color_trc) / sizeof(color_trc[0]); ++i) {
        actual = sixel_color_encode_srgb(color_trc[i][0]);
        if (!isfinite(actual) || fabs(actual - color_trc[i][2]) > 1e-12) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
