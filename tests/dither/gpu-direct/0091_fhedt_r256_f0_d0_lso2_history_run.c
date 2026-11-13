/* SPDX-License-Identifier: MIT */
/* Verify fhedt r256 f0 d0 lso2 history run. */
#include <sixel.h>
int test_direct_fhedt_case(int, int, int);

int
test_dither_gpu_direct_0091(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_fhedt_case(4, SIXEL_DIFFUSE_LSO2, 1);
}
