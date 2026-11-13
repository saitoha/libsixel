/* SPDX-License-Identifier: MIT */
/* Verify wire a rgb none. */
#include <sixel.h>
int test_gpupal_wire(int, int, int);

int
test_dither_a_dither_0039(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpupal_wire(SIXEL_DIFFUSE_A_DITHER, 0,
        SIXEL_LUT_POLICY_NONE);
}
