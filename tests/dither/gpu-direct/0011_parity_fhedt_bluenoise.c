/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpu_direct_parity_fhedt(int method, int ncolors, int resolution,
    int refine, int use_dist2);

int
test_gpu_direct_0011_parity_fhedt_bluenoise(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_BLUENOISE_DITHER, 256,
        64, 1, 0);
}
