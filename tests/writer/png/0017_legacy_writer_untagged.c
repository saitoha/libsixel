/* SPDX-License-Identifier: MIT */
#include "color_test_common.h"
int test_png_color_0017(int argc, char **argv)
{
    (void)argv;
    return argc > 1 ? png_color_check(-1, 0, 0)
                    : png_color_write("-", 0);
}
