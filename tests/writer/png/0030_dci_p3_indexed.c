/* SPDX-License-Identifier: MIT */
#include "color_test_common.h"

int
test_png_color_0027(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_color_check(3, 0, 0);
}
