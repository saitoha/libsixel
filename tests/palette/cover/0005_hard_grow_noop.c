/* SPDX-License-Identifier: MIT */
/* Hard mode and growth do not enable AUTO repair. */
#include <sixel.h>
int test_auto_cover_values(unsigned int, unsigned int, int, int, int);

int
test_auto_cover_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_values(32, 256, 0, 1,
        SIXEL_PIXELFORMAT_RGBFLOAT32);
}
