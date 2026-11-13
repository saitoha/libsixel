/* SPDX-License-Identifier: MIT */
/* Verify fhedt r64 f1 d1 lso2 history deferred. */
#include <sixel.h>
int test_direct_fhedt_case(int, int, int);

int
test_dither_gpu_direct_0077(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_fhedt_case(1, SIXEL_DIFFUSE_LSO2, 2);
}
