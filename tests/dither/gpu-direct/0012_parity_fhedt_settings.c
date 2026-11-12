/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpu_direct_parity_fhedt(int method, int ncolors, int resolution,
    int refine, int use_dist2);

int
test_gpu_direct_0012_parity_fhedt_settings(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    /* R128 refines, R256 skips refinement, R64 without it stores bytes,
     * and dist2 changes only the refined query. */
    if (test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_NONE, 256, 128, 1, 0) ||
            test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_X_DITHER, 256, 256, 1,
                0) ||
            test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_NONE, 256, 64, 0, 0) ||
            test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_A_DITHER, 16, 64, 1,
                1)) {
        return 1;
    }
    return 0;
}
