/* SPDX-License-Identifier: MIT */
/* 64 colors do not inherit the byte faces budget. */
#include <sixel.h>
int test_auto_cover_values(unsigned int, unsigned int, int, int, int);

int
test_auto_cover_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_values(64, 256, 1, 0,
        SIXEL_PIXELFORMAT_RGBFLOAT32);
}
