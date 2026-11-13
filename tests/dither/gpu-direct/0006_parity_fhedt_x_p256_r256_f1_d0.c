/* SPDX-License-Identifier: MIT */
/* Verify parity fhedt x p256 r256 f1 d0. */
#include <sixel.h>
int test_gpu_direct_parity_fhedt(int, int, int, int, int);

int
test_dither_gpu_direct_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_X_DITHER, 256, 256, 1,
        0);
}
