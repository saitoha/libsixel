/* SPDX-License-Identifier: MIT */
/* Verify atkinson device retained partial run matches CPU history. */
#include <sixel.h>
int test_diffusion_gpu_retained(int, int, int);

int
test_dither_atkinson_gpu_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_diffusion_gpu_retained(SIXEL_DIFFUSE_ATKINSON, 1,
        0);
}
