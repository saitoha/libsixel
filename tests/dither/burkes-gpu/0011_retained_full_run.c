/* SPDX-License-Identifier: MIT */
/* Verify burkes device retained full run matches CPU history. */
#include <sixel.h>
int test_diffusion_gpu_retained(int, int, int);

int
test_dither_burkes_gpu_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_diffusion_gpu_retained(SIXEL_DIFFUSE_BURKES, 0,
        0);
}
