/* SPDX-License-Identifier: MIT */
/* Verify wire rgb none. */
#include <sixel.h>
int test_gpupal_wire(int, int, int);

int
test_gpupal_0005_wire_rgb_none(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpupal_wire(
        SIXEL_DIFFUSE_NONE, 0, SIXEL_LUT_POLICY_NONE);
}
