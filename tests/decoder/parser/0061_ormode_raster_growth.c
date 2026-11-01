/* SPDX-License-Identifier: MIT */
/* Plane words survive width and band growth beyond attributed dimensions. */
#include "../../_support/processing/decoder/ormode_test_common.h"

int
test_or_dec_0056(int argc, char **argv)
{
    unsigned short const expected[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 3, 2, 2,
    };
    unsigned char const rgba[] = {
        255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255, 0, 0, 255, 255, 0, 255, 0, 255,
        0, 255, 0, 255,
    };
    char const *stream =
        "\033P7;5q\"1;1;1;1#0;2;0;0;0#1;2;100;0;0#2;2;0;100;0#3;2;0;0;100#"
        "1!3~-#2!3@$#1@\033\\";

    (void)argc;
    (void)argv;
    return test_or_planar(stream, expected, rgba, 3, 7, 4);
}
