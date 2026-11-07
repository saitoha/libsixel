/* SPDX-License-Identifier: MIT */
/* A unit-aspect repair must preserve the baseline's expanded rows. */
#include "../../_support/processing/decoder/multi_pass_test_common.h"

int
test_decoder_aspect_0020(int argc, char **argv)
{
    static char const stream[] =
        "\033P0;1q\"2;1;1;3#1;2;100;0;0#1B\033\\"
        "\033P0;1q\"1;1;1;3#1;2;0;100;0#1A\033\\";
    static unsigned char const expected[] = {
        255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255
    };

    (void)argc;
    (void)argv;
    return test_multi_pass_capture(stream, sizeof(stream) - 1U,
        expected, 1, 3, 2U, "");
}
