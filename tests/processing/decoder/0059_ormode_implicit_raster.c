/* SPDX-License-Identifier: MIT */
/* Without raster attributes, wide planes grow and crop to painted bounds. */
#include "ormode_test_common.h"

int
test_or_dec_0059(int argc, char **argv)
{
    unsigned short const expected[] = {
        256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
        256, 256, 256, 256, 256, 256, 257, 257, 257,
    };
    unsigned char const rgba[] = {
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255,
    };
    char const *stream =
        "\033P7;5q#0;2;0;0;0#257;2;100;0;0#256!3~-#256!3@$#1!3@\033\\";

    (void)argc;
    (void)argv;
    return test_or_planar(stream, expected, rgba, 3, 7, 258);
}
