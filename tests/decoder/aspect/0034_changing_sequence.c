/* SPDX-License-Identifier: MIT */
/* Packet validation and composition use the changing output-row cursor. */
#include "../../_support/processing/decoder/multi_pass_test_common.h"

int
test_decoder_aspect_0034(int argc, char **argv)
{
    static char const stream[] =
        "\033P0;1q\"1;1;1;8#1;2;100;0;0#1~-#1B\033\\"
        "\033P0;1q\"1;1;1;8#2;2;0;100;0#2@-\"2;1;1;8#2@\033\\";
    static unsigned char const expected[] = {
        0, 255, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255
    };

    (void)argc;
    (void)argv;
    return test_multi_pass_capture(stream, sizeof(stream) - 1,
        expected, 1, 8, 2U, "");
}
