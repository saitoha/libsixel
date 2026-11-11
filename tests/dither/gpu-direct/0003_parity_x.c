/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpu_direct_parity(int method, int ncolors);

int
test_gpu_direct_0003_parity_x(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity(SIXEL_DIFFUSE_X_DITHER, 256);
}
