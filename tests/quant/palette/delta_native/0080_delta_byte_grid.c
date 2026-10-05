/* SPDX-License-Identifier: MIT */
/* Exhaust the byte grid with an independent nearest-grid reference. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"

int
test_delta_native_0080(int argc, char **argv)
{
    unsigned char history[3];
    unsigned char query[3];
    unsigned char chosen[3];
    unsigned char valid[1] = {1};
    sixel_dither_t *dither;
    int value;
    int grid;
    int percent;
    int expected;
    int distance;
    int best;
    int endpoint;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 3, NULL))) {
        goto end;
    }
    for (value = 0; value <= 255; ++value) {
        /* Enumerate exact rational distances instead of duplicating the
         * production rounding formula or its lookup table. */
        best = INT_MAX;
        percent = 0;
        for (grid = 0; grid <= 100; ++grid) {
            distance = abs(value * 100 - grid * 255);
            if (distance < best) {
                best = distance;
                percent = grid;
            }
        }
        best = INT_MAX;
        expected = 0;
        for (grid = 0; grid <= 255; ++grid) {
            distance = abs(percent * 255 - grid * 100);
            /* An exact half-byte chooses the larger byte. */
            if (distance <= best) {
                best = distance;
                expected = grid;
            }
        }
        memset(history, expected, sizeof(history));
        memset(chosen, value, sizeof(chosen));
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 2, 1, 0, SIXEL_6DELTA_ERROR_SKIP);
        dither->pipeline_6delta_rgb101 = 1;
        /* Against query 0, keep requires expected <= projected candidate.
         * Against query 255 it requires >=. Both prove exact equality. */
        for (endpoint = 0; endpoint <= 1; ++endpoint) {
            memset(query, endpoint * 255, sizeof(query));
            if (!sixel_dither_pipeline_6delta_try_keep_after_lookup(dither,
                    0, 0, 0, query, chosen, 0, NULL, NULL)) {
                fprintf(stderr, "byte grid=%d expected=%d endpoint=%d\n",
                    value, expected, endpoint);
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
