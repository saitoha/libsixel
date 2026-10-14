/* SPDX-License-Identifier: MIT */
/* Verify wire rgb bluenoise. */
#include <sixel.h>
int test_gpupal_wire(int, int, int);

int
test_gpupal_0006_wire_rgb_bluenoise(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpupal_wire(
        SIXEL_DIFFUSE_BLUENOISE_DITHER, 0, SIXEL_LUT_POLICY_NONE);
}
