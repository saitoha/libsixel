/* SPDX-License-Identifier: MIT */
/* 32 colors retain exact float entries without histogram allocation. */
#include <sixel.h>
int test_auto_cover_values(unsigned int, unsigned int, int, int, int);

int
test_auto_cover_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_values(32, 256, 1, 0,
        SIXEL_PIXELFORMAT_RGBFLOAT32);
}
