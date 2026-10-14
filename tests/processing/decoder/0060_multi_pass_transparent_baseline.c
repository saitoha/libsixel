/* SPDX-License-Identifier: MIT */
/* Verify transparent baseline. */
#include "multi_pass_test_common.h"

int test_sequence_0060(int argc, char **argv)
{
    static char const stream[] =
        "\0337\033P0;1q\"1;1;1;1\033\\";
    static unsigned char const expected[] = {
        0, 0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_multi_pass_capture(stream, sizeof(stream) - 1U,
        expected, 1, 1, 1U, "");
}
