/* SPDX-License-Identifier: MIT */
/* Explicit caps retain encoder reservations; geometry remains independent. */
#include <stdlib.h>
#include "src/planner.h"

int
test_allocation_controls(int argc, char **argv)
{
    sixel_dither_encode_request_t request = {0};
    sixel_dither_encode_allocation_t allocation;
    int ok;

    (void)argc;
    (void)argv;
    request.total_threads = 4;
    request.height = 480;
    request.ncolors = 16;
    request.method_for_diffuse = SIXEL_DIFFUSE_FS;
    request.prefer_staged = 1;
    request.threads_max_set = 1;
    request.threads_max = 1;
    request.band_height_set = 1;
    request.band_height = 9;
    request.overlap_set = 1;
    request.overlap = 0;
    sixel_dither_encode_allocate(&request, &allocation);
    ok = allocation.mode == SIXEL_DITHER_ENCODE_PIPELINE;
    ok &= allocation.dither_threads == 1 && allocation.encode_threads == 3;
    ok &= allocation.band_height == 12 && allocation.overlap == 0;
    request.threads_max = 99;
    request.overlap = 99;
    sixel_dither_encode_allocate(&request, &allocation);
    ok &= allocation.dither_threads == 2 && allocation.encode_threads == 2;
    ok &= allocation.band_height == 12 && allocation.overlap == 6;
    request.threads_max_set = 0;
    request.band_height = 1;
    sixel_dither_encode_allocate(&request, &allocation);
    ok &= allocation.band_height == 6 && allocation.overlap == 3;
    request.overlap = 0;
    sixel_dither_encode_allocate(&request, &allocation);
    ok &= allocation.band_height == 6 && allocation.overlap == 0;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
