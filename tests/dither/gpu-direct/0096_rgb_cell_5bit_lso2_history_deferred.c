/* SPDX-License-Identifier: MIT */
/* Verify rgb cell 5bit lso2 history deferred. */
#include <sixel.h>
int test_direct_rgb_cell_case(int, int, int);

int
test_dither_gpu_direct_0096(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_rgb_cell_case(0, SIXEL_DIFFUSE_LSO2, 2);
}
