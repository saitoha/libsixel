/* SPDX-License-Identifier: MIT */
/* Pipeline slots add to T; staged reservations belong to disjoint phases. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include "src/planner.h"

int
test_allocation_slots(int argc, char **argv)
{
    static int const budgets[] = {1, 2, 3, 4, 6, 8, 10, 14};
    static int const dithers[] = {1, 1, 1, 2, 4, 6, 7, 10};
    static int const encoders[] = {1, 2, 2, 2, 2, 2, 3, 4};
    static int const bands[] = {0, 0, 480, 240, 120, 84, 72, 48};
    static int const staged_bands[] = {0, 240, 162, 120, 84, 60, 48, 36};
    sixel_dither_encode_request_t request = {0};
    sixel_dither_encode_allocation_t allocation;
    int i;
    int stage;
    int effective_stage;
    int ok;

    (void)argc;
    (void)argv;
    request.height = 480;
    request.ncolors = 16;
    request.method_for_diffuse = SIXEL_DIFFUSE_FS;
    ok = 1;
    for (stage = 0; stage < 2; ++stage) {
        request.prefer_staged = stage;
        for (i = 0; i < 8; ++i) {
            request.total_threads = budgets[i];
            effective_stage = stage && budgets[i] > 1;
#if !SIXEL_ENABLE_THREADS || defined(__PCC__) || defined(__TINYC__)
            effective_stage = 0;
#endif
            sixel_dither_encode_allocate(&request, &allocation);
            ok &= allocation.dither_threads ==
                (effective_stage ? budgets[i] : dithers[i]);
            ok &= allocation.encode_threads ==
                (effective_stage ? budgets[i] : encoders[i]);
            ok &= allocation.band_height ==
                (effective_stage ? staged_bands[i] : bands[i]);
            ok &= allocation.mode == (effective_stage
                ? SIXEL_DITHER_ENCODE_STAGED : budgets[i] <= 2
                    ? SIXEL_DITHER_ENCODE_SERIAL
                    : SIXEL_DITHER_ENCODE_PIPELINE);
        }
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
