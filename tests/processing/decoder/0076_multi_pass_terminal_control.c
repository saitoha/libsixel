/* SPDX-License-Identifier: MIT */
/* Verify terminal control. */
#include "multi_pass_test_common.h"

int test_sequence_0076(int argc, char **argv)
{
    static char const stream[] =
        "\033P0;1q\"1;1;3;1#0;2;100;0;0@@@\033\\\033[H\033P0;1q"
        "\"1;1;3;1#0;2;0;0;100?@\033\\";

    (void)argc;
    (void)argv;
    return test_multi_pass_capture(stream, sizeof(stream) - 1U,
        NULL, 0, 0, 0U, "");
}
