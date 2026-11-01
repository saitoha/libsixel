/* SPDX-License-Identifier: MIT */
/* Verify paint width. */
#include "../../_support/processing/decoder/multi_pass_test_common.h"

int test_sequence_0065(int argc, char **argv)
{
    static char const stream[] =
        "\033P0;1q\"1;1;3;1#0;2;100;0;0@@@@\033\\";

    (void)argc;
    (void)argv;
    return test_multi_pass_capture(stream, sizeof(stream) - 1U,
        NULL, 0, 0, 0U, "");
}
