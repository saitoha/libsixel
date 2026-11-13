/* SPDX-License-Identifier: MIT */
/* Verify lso2 palette256. */
#include <sixel.h>
int test_direct_lso2_palette256(void);

int
test_dither_gpu_direct_0029(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_lso2_palette256();
}
