/* SPDX-License-Identifier: MIT */
/* AUTO preserves typed linear coordinates without a round trip. */
#include <sixel.h>
int test_auto_cover_values(unsigned int, unsigned int, int, int, int);

int
test_auto_cover_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_values(128, 256, 1, 0,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
}
