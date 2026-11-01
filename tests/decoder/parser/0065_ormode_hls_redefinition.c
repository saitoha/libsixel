/* SPDX-License-Identifier: MIT */
/* OR resolves HLS definitions after painting, including palette zero. */
#include "../../_support/processing/decoder/ormode_test_common.h"

int
test_or_dec_0058(int argc, char **argv)
{
    unsigned short const expected[] = {
        3, 0,
    };
    unsigned char const rgba[] = {
        255, 0, 0, 255, 0, 0, 255, 255,
    };
    char const *stream =
        "\033P7;5q\"1;1;2;1#1;2;0;100;0#1@$#2@#0;1;0;50;100#3;1;120;50;100"
        "$\033\\";

    (void)argc;
    (void)argv;
    return test_or_planar(stream, expected, rgba, 2, 1, 4);
}
