/* SPDX-License-Identifier: MIT */
/* Verify sierra1 device retained full resolve matches CPU history. */
#include <sixel.h>
int test_diffusion_gpu_retained(int, int, int);

int
test_dither_sierra1_gpu_0013(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_diffusion_gpu_retained(SIXEL_DIFFUSE_SIERRA1, 0,
        1);
}
