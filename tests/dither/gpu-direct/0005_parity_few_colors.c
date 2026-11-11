/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpu_direct_parity(int method, int ncolors);

int
test_gpu_direct_0005_parity_few_colors(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity(SIXEL_DIFFUSE_A_DITHER, 7);
}
