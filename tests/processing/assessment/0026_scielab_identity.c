/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "assessment/scielab.h"

int
test_qa_0026(int argc, char **argv)
{
    double image[5 * 4 * 3];
    double mean;
    double maximum;
    double map[5 * 4];
    int i;

    (void)argc;
    (void)argv;
    for (i = 0; i < 60; ++i) {
        image[i] = (i * 17 % 61 - 5) / 32.0;
    }
    if (SIXEL_FAILED(sixel_qa_scielab(image, image, 5, 4, 60,
            &mean, &maximum, map)) || mean != 0 || maximum != 0) {
        return EXIT_FAILURE;
    }
    for (i = 0; i < 20; ++i) {
        if (map[i] != 0) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
