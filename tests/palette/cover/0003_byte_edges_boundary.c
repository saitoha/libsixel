/* SPDX-License-Identifier: MIT */
/* 256 colors do not inherit the byte edges budget. */
#include <sixel.h>
int test_auto_cover_values(unsigned int, unsigned int, int, int, int);

int
test_auto_cover_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_values(256, 256, 1, 0,
        SIXEL_PIXELFORMAT_RGBFLOAT32);
}
