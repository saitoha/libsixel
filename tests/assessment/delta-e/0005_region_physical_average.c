/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0005(int argc, char **argv)
{
    double a[64 * 64 * 3];
    double b[64 * 64 * 3];
    double white[3] = {.9642, 1, .8249};
    double mean;
    double maximum;
    size_t count;
    int i;
    int c;
    int window;
    (void)argc;
    (void)argv;
    /* A half-black/half-white dither has the same physical mean as Y=.5.
     * Averaging encoded RGB, Lab or per-pixel delta cannot pass this test. */
    for (i = 0; i < 64 * 64; ++i) {
        for (c = 0; c < 3; ++c) {
            a[i * 3 + c] = (i % 2) * white[c];
            b[i * 3 + c] = .5 * white[c];
        }
    }
    for (window = 8; window <= 64; window *= 2) {
        sixel_qa_regions(a, b, 64, 64, window, &mean, &maximum, &count);
        if (mean >= 1e-10 || maximum >= 1e-10
                || count != (size_t)(4096 / (window * window))) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
