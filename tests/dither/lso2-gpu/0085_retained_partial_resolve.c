/* SPDX-License-Identifier: MIT */
/* Verify lso2 device retained partial resolve matches CPU history. */
#include <sixel.h>
int test_diffusion_gpu_retained(int, int, int);

int
test_dither_lso2_gpu_0085(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_diffusion_gpu_retained(SIXEL_DIFFUSE_LSO2, 1,
        1);
}
