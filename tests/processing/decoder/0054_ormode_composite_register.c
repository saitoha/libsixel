/* SPDX-License-Identifier: MIT */
/* Composite registers combine all selected bits, including bit 7. */
#include "ormode_test_common.h"

int
test_or_dec_0054(int argc, char **argv)
{
    unsigned short const expected[] = {
        7, 131, 255, 7, 131, 255, 7, 131, 255, 7, 131, 255,
        7, 131, 255, 7, 131, 255, 7, 131, 255,
    };
    unsigned char const rgba[] = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255,
        0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255,
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255,
        0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255,
    };
    char const *stream =
        "\033P7;5q\"1;1;3;7#0;2;0;0;0#7;2;100;0;0#131;2;0;100;0#255;2;0;0;"
        "100#3!3~$#4~#128~#252~-#3!3@$#4@#128@#252@\033\\";

    (void)argc;
    (void)argv;
    return test_or_planar(stream, expected, rgba, 3, 7, 256);
}
