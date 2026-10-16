/* SPDX-License-Identifier: MIT */
/* Force donation after the first batch without elapsed-time assumptions.
 * A later overflow has a different diagnostic from the earlier NaN, so the
 * caller must select the first failing chunk rather than completion order. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/threading.h"

static int
reclaim_11_after_batch(void *context)
{
    int *polls;

    polls = (int *)context;
    return ++*polls == 2;
}

int
test_parallel_float_0011(int argc, char **argv)
{
    float *pixels;
    float *serial;
    sixel_thread_budget_scope_t scope;
    SIXELSTATUS status;
    size_t size;
    int i;
    int polls;
    int initial;
    int ok;

    (void)argc;
    (void)argv;
    size = 131073U * 3U * sizeof(float);
    pixels = malloc(size);
    serial = malloc(size);
    if (pixels == NULL || serial == NULL) {
        free(pixels);
        free(serial);
        return EXIT_FAILURE;
    }
    for (i = 0; i < 131073 * 3; ++i) {
        pixels[i] = (float)(i % 251) / 250.0f;
    }
    pixels[65536 * 3] = NAN;
    pixels[131072 * 3] = FLT_MAX;
    memcpy(serial, pixels, size);
    sixel_set_threads(1);
    status = sixel_helper_convert_colorspace((unsigned char *)serial, size,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_COLORSPACE_GAMMA,
        SIXEL_COLORSPACE_LINEAR);
    ok = status == SIXEL_BAD_INPUT;
    for (initial = 1; initial <= 4; initial *= 2) {
        for (i = 0; i < 131073 * 3; ++i) {
            pixels[i] = (float)(i % 251) / 250.0f;
        }
        pixels[65536 * 3] = NAN;
        pixels[131072 * 3] = FLT_MAX;
        polls = 0;
        sixel_set_threads(8);
        sixel_thread_budget_enter(&scope, initial);
        sixel_thread_budget_set_reclaim(&scope, 8,
            reclaim_11_after_batch, &polls);
        status = sixel_helper_convert_colorspace((unsigned char *)pixels, size,
            SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_COLORSPACE_GAMMA,
            SIXEL_COLORSPACE_LINEAR);
        sixel_thread_budget_leave(&scope);
        ok &= polls == 2 && status == SIXEL_BAD_INPUT &&
            memcmp(pixels, serial, size) == 0;
        ok &= strcmp(sixel_helper_get_additional_message(),
            "float frame conversion: nonfinite or domain error.") == 0;
    }
    free(serial);
    free(pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
