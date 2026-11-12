/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpu_direct_parity_fhedt(int method, int ncolors, int resolution,
    int refine, int use_dist2);

int
test_gpu_direct_0010_parity_fhedt(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_NONE, 256, 64, 1, 0);
}
