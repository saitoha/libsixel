/* SPDX-License-Identifier: MIT */
/* Verify wire hls none. */
#include <sixel.h>
int test_gpupal_wire(int, int, int);

int
test_gpupal_0008_wire_hls_none(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpupal_wire(
        SIXEL_DIFFUSE_NONE, 1, SIXEL_LUT_POLICY_NONE);
}
