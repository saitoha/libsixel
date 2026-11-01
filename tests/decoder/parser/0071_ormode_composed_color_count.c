/* SPDX-License-Identifier: MIT */
/* The maximum composed index need not occur as a selected register. */
#include "../../_support/processing/decoder/ormode_test_common.h"

int
test_or_dec_0061(int argc, char **argv)
{
    unsigned short const expected[] = { 255, 255, 255, 255, 255, 255 };
    unsigned char const rgba[] = {
        253, 253, 253, 255, 253, 253, 253, 255, 253, 253, 253, 255,
        253, 253, 253, 255, 253, 253, 253, 255, 253, 253, 253, 255
    };
    char const *stream = "\033P7;5q\"1;1;1;6#128~$#127~\033\\";

    (void)argc;
    (void)argv;
    return test_or_planar(stream, expected, rgba, 1, 6, 256);
}
