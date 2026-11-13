/* SPDX-License-Identifier: MIT */
/* Verify burkes deadzone raster. */
#include <sixel.h>
int test_direct_carry_case(int, int);

int
test_dither_gpu_direct_0049(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_carry_case(4, 0);
}
