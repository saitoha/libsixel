/* SPDX-License-Identifier: MIT */
#include "color_test_common.h"
int test_png_color_0030(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_snapshot_check(2, 0);
}
