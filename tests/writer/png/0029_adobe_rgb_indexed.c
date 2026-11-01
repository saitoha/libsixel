/* SPDX-License-Identifier: MIT */
#include "color_test_common.h"

int
test_png_color_0026(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_color_check(2, 0, 0);
}
