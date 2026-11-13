/* SPDX-License-Identifier: MIT */
/* Verify sierra2 deadzone serpentine. */
#include <sixel.h>
int test_direct_carry_case(int, int);

int
test_dither_gpu_direct_0060(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_carry_case(6, 1);
}
