/* SPDX-License-Identifier: MIT */
/* Verify parity fhedt none p256 r64 f0 d0. */
#include <sixel.h>
int test_gpu_direct_parity_fhedt(int, int, int, int, int);

int
test_dither_gpu_direct_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_NONE, 256, 64, 0, 0);
}
