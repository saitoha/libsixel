/* SPDX-License-Identifier: MIT */
/* Verify wire rgb eytzinger. */
#include <sixel.h>
int test_gpupal_wire(int, int, int);

int
test_gpupal_0007_wire_rgb_eytzinger(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpupal_wire(
        SIXEL_DIFFUSE_NONE, 0, SIXEL_LUT_POLICY_EYTZINGER);
}
