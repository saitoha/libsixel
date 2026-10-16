/* SPDX-License-Identifier: MIT */
/* Force donation after the first batch without elapsed-time assumptions. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/threading.h"

static int
reclaim_10_after_batch(void *context)
{
    int *polls;

    polls = (int *)context;
    return ++*polls == 2;
}

int
test_parallel_float_0010(int argc, char **argv)
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
    memcpy(serial, pixels, size);
    sixel_set_threads(1);
    status = sixel_helper_convert_colorspace((unsigned char *)serial, size,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_COLORSPACE_GAMMA,
        SIXEL_COLORSPACE_CIELAB);
    ok = SIXEL_SUCCEEDED(status);
    for (initial = 1; initial <= 4; initial *= 2) {
        for (i = 0; i < 131073 * 3; ++i) {
            pixels[i] = (float)(i % 251) / 250.0f;
        }
        polls = 0;
        sixel_set_threads(8);
        sixel_thread_budget_enter(&scope, initial);
        sixel_thread_budget_set_reclaim(&scope, 8,
            reclaim_10_after_batch, &polls);
        status = sixel_helper_convert_colorspace((unsigned char *)pixels, size,
            SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_COLORSPACE_GAMMA,
            SIXEL_COLORSPACE_CIELAB);
        sixel_thread_budget_leave(&scope);
        ok &= polls == 2 && SIXEL_SUCCEEDED(status) &&
            memcmp(pixels, serial, size) == 0;
    }
    free(serial);
    free(pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
