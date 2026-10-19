/* SPDX-License-Identifier: MIT */
/* Frame-global and unknown policies retain their existing overlap budget. */
#include <stdlib.h>
#include "src/planner.h"

int
test_allocation_serial_policy(int argc, char **argv)
{
    sixel_dither_encode_request_t request = {0};
    sixel_dither_encode_allocation_t allocation;
    int ok;

    (void)argc;
    (void)argv;
    request.total_threads = 3;
    request.height = 480;
    request.ncolors = 16;
    request.prefer_staged = 1;
    request.method_for_diffuse = SIXEL_DIFFUSE_INTERFRAME;
    sixel_dither_encode_allocate(&request, &allocation);
    ok = allocation.mode == SIXEL_DITHER_ENCODE_PIPELINE;
    ok &= allocation.dither_threads == 1 && allocation.encode_threads == 2;
    request.method_for_diffuse = -1;
    sixel_dither_encode_allocate(&request, &allocation);
    ok &= allocation.mode == SIXEL_DITHER_ENCODE_PIPELINE;
    ok &= allocation.dither_threads == 1 && allocation.encode_threads == 2;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
