/* SPDX-License-Identifier: MIT */
/* AUTO growth preserves the reserved slot and 255 entries. */
#include <sixel.h>
int test_auto_cover_values(unsigned int, unsigned int, int, int, int);

int
test_auto_cover_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_values(255, 255, 1, 1,
        SIXEL_PIXELFORMAT_RGBFLOAT32);
}
