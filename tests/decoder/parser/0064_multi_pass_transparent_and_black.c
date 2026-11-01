/* SPDX-License-Identifier: MIT */
/* Verify transparent and black. */
#include "../../_support/processing/decoder/multi_pass_test_common.h"

int test_sequence_0058(int argc, char **argv)
{
    static char const stream[] =
        "\033P0;1q\"1;1;3;1#0;2;0;0;0@\033\\\033P0;1q\"1;1;3;"
        "1#0;2;0;0;100?@\033\\";
    static unsigned char const expected[] = {
        0, 0, 0, 255, 0, 0, 255, 255, 0, 0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_multi_pass_capture(stream, sizeof(stream) - 1U,
        expected, 3, 1, 2U, "");
}
