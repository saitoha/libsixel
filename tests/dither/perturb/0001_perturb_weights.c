/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sixel.h>
#include "src/dither.h"

/* Paired integer and float weights retain unit gain without negative taps. */
int
test_perturb_0001_weights(int argc, char **argv)
{
    sixel_dither_perturb_t context;
    float amounts[3] = { 0.25f, 0.5f, 1.0f };
    float weights[4];
    int num[4];
    int i;
    int j;
    int k;
    int x;
    int y;
    uint32_t state;

    (void)argc;
    (void)argv;
    state = 123456789u;
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 10000; ++j) {
            /* Test-only pseudorandom coordinates include nonzero band rows. */
            state = state * 1664525u + 1013904223u;
            x = (int)(state & 0x7fffffffu);
            state = state * 1664525u + 1013904223u;
            y = (int)(state & 0x7fffffffu);
            sixel_dither_perturb_init(&context, amounts[i], -j);
            sixel_dither_perturb_weights(&context, x, y, num);
            sixel_dither_perturb_float(&context, x, y, weights);
            if (num[0] + num[2] != (12 << 8)
                    || num[1] + num[3] != (4 << 8)
                    || fabsf(weights[0] + weights[2] - 0.75f) > 1e-7f
                    || fabsf(weights[1] + weights[3] - 0.25f) > 1e-7f) {
                return 1;
            }
            for (k = 0; k < 4; ++k) {
                if (num[k] < 0 || weights[k] < 0.0f) {
                    return 1;
                }
            }
        }
    }
    return 0;
}
