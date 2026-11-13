/* SPDX-License-Identifier: MIT */
/* Verify parity fhedt a p16 r64 f1 d1. */
#include <sixel.h>
int test_gpu_direct_parity_fhedt(int, int, int, int, int);

int
test_dither_gpu_direct_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity_fhedt(SIXEL_DIFFUSE_A_DITHER, 16, 64, 1,
        1);
}
