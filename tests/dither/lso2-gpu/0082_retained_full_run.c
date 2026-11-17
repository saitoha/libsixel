/* SPDX-License-Identifier: MIT */
/* Verify lso2 device retained full run matches CPU history. */
#include <sixel.h>
int test_diffusion_gpu_retained(int, int, int);

int
test_dither_lso2_gpu_0082(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_diffusion_gpu_retained(SIXEL_DIFFUSE_LSO2, 0,
        0);
}
