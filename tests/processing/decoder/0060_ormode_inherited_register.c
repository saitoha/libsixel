/* SPDX-License-Identifier: MIT */
/* Workers reconstruct the selected register inherited from an earlier band. */
#include "ormode_test_common.h"

int
test_or_dec_0060(int argc, char **argv)
{
    unsigned short const expected[] = {
        3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
    };
    unsigned char const rgba[] = {
        0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
        0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
        0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255
    };
    char const *stream =
        "\033P7;5q\"1;1;1;24#0;2;0;0;0#1;2;100;0;0#2;2;0;100;0"
        "#3;2;0;0;100#1~$#2~-~-~-~\033\\";

    (void)argc;
    (void)argv;
    return test_or_planar(stream, expected, rgba, 1, 24, 4);
}
