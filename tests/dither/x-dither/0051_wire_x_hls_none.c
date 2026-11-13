/* SPDX-License-Identifier: MIT */
/* Verify wire x hls none. */
#include <sixel.h>
int test_gpupal_wire(int, int, int);

int
test_dither_x_dither_0051(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpupal_wire(SIXEL_DIFFUSE_X_DITHER, 1,
        SIXEL_LUT_POLICY_NONE);
}
