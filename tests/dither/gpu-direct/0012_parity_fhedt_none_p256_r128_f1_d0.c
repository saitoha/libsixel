/* SPDX-License-Identifier: MIT */
/* Verify parity fhedt none p256 r128 f1 d0. */
#include <sixel.h>
int test_gpu_direct_parity_fhedt(int, int, int, int, int);

int
test_dither_gpu_direct_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_NONE, 256, 128, 1, 0);
}
