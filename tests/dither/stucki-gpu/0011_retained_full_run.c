/* SPDX-License-Identifier: MIT */
/* Verify stucki device retained full run matches CPU history. */
#include <sixel.h>
int test_diffusion_gpu_retained(int, int, int);

int
test_dither_stucki_gpu_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_diffusion_gpu_retained(SIXEL_DIFFUSE_STUCKI, 0,
        0);
}
