/* SPDX-License-Identifier: MIT */
/* Invalid or unrepresentable decoding leaves the entire input unchanged. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"

int
test_cms_0019(int argc, char **argv)
{
    float pixels[6];
    float original[6];
    float invalid[4];
    size_t i;

    (void)argc;
    (void)argv;
    invalid[0] = NAN;
    invalid[1] = INFINITY;
    invalid[2] = -INFINITY;
    invalid[3] = FLT_MAX;
    for (i = 0; i < 4; ++i) {
        pixels[0] = 0.5f;
        pixels[1] = -0.2f;
        pixels[2] = 1.2f;
        pixels[3] = 0.0f;
        pixels[4] = 1.0f;
        pixels[5] = invalid[i];
        memcpy(original, pixels, sizeof(pixels));
        if (sixel_cms_convert_rgbf32_gamma_to_linear(pixels, 2u) ||
            memcmp(original, pixels, sizeof(pixels)) != 0) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
