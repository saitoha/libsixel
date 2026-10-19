/* SPDX-License-Identifier: MIT */
/* The four-slot boundary and effective method determine automatic staging. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include "src/planner.h"

int
test_allocation_automatic_policy(int argc, char **argv)
{
    static int const budgets[] = {1, 2, 3, 4, 5, 6, 8, 10, 14};
    static int const stages[] = {0, 1, 1, 1, 0, 0, 0, 0, 0};
    static int const methods[] = {
        SIXEL_DIFFUSE_AUTO, SIXEL_DIFFUSE_FS, SIXEL_DIFFUSE_ATKINSON,
        SIXEL_DIFFUSE_JAJUNI, SIXEL_DIFFUSE_STUCKI, SIXEL_DIFFUSE_BURKES,
        SIXEL_DIFFUSE_SIERRA1, SIXEL_DIFFUSE_SIERRA2, SIXEL_DIFFUSE_SIERRA3,
        SIXEL_DIFFUSE_LSO2, SIXEL_DIFFUSE_A_DITHER, SIXEL_DIFFUSE_X_DITHER,
        SIXEL_DIFFUSE_BLUENOISE_DITHER, SIXEL_DIFFUSE_NONE,
        SIXEL_DIFFUSE_INTERFRAME, -1
    };
    sixel_dither_encode_request_t request = {0};
    sixel_dither_encode_allocation_t allocation;
    int i;
    int j;
    int selected;
    int expected;
    int ok;

    (void)argc;
    (void)argv;
    request.height = 480;
    request.ncolors = 16;
    ok = 1;
    for (i = 0; i < 9; ++i) {
        for (j = 0; j < 16; ++j) {
            request.total_threads = budgets[i];
            request.method_for_diffuse = methods[j];
            selected = sixel_dither_encode_default_staged(budgets[i],
                methods[j]);
            expected = stages[i] && j < 13;
            ok &= selected == expected;
            request.prefer_staged = selected;
            sixel_dither_encode_allocate(&request, &allocation);
#if !SIXEL_ENABLE_THREADS || defined(__PCC__) || defined(__TINYC__)
            expected = 0;
#endif
            ok &= (allocation.mode == SIXEL_DITHER_ENCODE_STAGED)
                == expected;
            if (expected) {
                ok &= allocation.dither_threads == budgets[i];
                ok &= allocation.encode_threads == budgets[i];
            }
            /* Stateless application needs no warm-up history. Diffusion
             * retains six rows when low-color geometry is active. */
            if (allocation.band_height > 0) {
                ok &= allocation.overlap == (j >= 10 && j <= 13 ? 0 : 6);
            }
        }
    }
    request.total_threads = 4;
    request.ncolors = 256;
    request.method_for_diffuse = SIXEL_DIFFUSE_FS;
    request.prefer_staged = 1;
    sixel_dither_encode_allocate(&request, &allocation);
    ok &= allocation.overlap == 0;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
