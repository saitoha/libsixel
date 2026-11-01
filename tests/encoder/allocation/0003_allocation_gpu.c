/* SPDX-License-Identifier: MIT */
/* A GPU apply claim keeps all CPU slots for encode, ahead of CPU controls. */
#include <stdlib.h>
#include "src/planner.h"

int
test_allocation_gpu(int argc, char **argv)
{
    sixel_dither_encode_request_t request = {0};
    sixel_dither_encode_allocation_t allocation;
    int threads;
    int ok;

    (void)argc;
    (void)argv;
    request.height = 480;
    request.ncolors = 16;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.prefer_staged = 1;
    request.gpu_encode_only = 1;
    request.threads_max_set = 1;
    request.threads_max = 1;
    request.band_height_set = 1;
    request.band_height = 480;
    request.overlap_set = 1;
    request.overlap = 99;
    ok = 1;
    for (threads = 2; threads <= 14; ++threads) {
        request.total_threads = threads;
        sixel_dither_encode_allocate(&request, &allocation);
        ok &= allocation.mode == SIXEL_DITHER_ENCODE_PIPELINE;
        ok &= allocation.dither_threads == 0;
        ok &= allocation.encode_threads == threads;
        ok &= allocation.band_height == 6 && allocation.overlap == 0;
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
