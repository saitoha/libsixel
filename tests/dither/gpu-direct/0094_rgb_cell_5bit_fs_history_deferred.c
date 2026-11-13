/* SPDX-License-Identifier: MIT */
/* Verify rgb cell 5bit fs history deferred. */
#include <sixel.h>
int test_direct_rgb_cell_case(int, int, int);

int
test_dither_gpu_direct_0094(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_rgb_cell_case(0, SIXEL_DIFFUSE_FS, 2);
}
