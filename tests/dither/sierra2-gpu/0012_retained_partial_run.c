/* SPDX-License-Identifier: MIT */
/* Verify sierra2 device retained partial run matches CPU history. */
#include <sixel.h>
int test_diffusion_gpu_retained(int, int, int);

int
test_dither_sierra2_gpu_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_diffusion_gpu_retained(SIXEL_DIFFUSE_SIERRA2, 1,
        0);
}
