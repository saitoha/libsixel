/* SPDX-License-Identifier: MIT */
/* Verify skip outside. */
#include "multi_pass_test_common.h"

int test_sequence_0081(int argc, char **argv)
{
    static char const stream[] =
        "\033P0;1q\"1;1;3;1#0;2;100;0;0@@@!9?-----?\033\\";
    static unsigned char const expected[] = {
        255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255
    };

    (void)argc;
    (void)argv;
    return test_multi_pass_capture(stream, sizeof(stream) - 1U,
        expected, 3, 1, 1U, "");
}
