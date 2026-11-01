/* SPDX-License-Identifier: MIT */
/* Wide registers compose through all sixteen planes and final palette
 * lookup. */
#include "../../_support/processing/decoder/ormode_test_common.h"

int
test_or_dec_0055(int argc, char **argv)
{
    unsigned short const expected[] = {
        259, 33025, 65535, 259, 33025, 65535, 259, 33025, 65535, 259,
        33025, 65535,
        259, 33025, 65535, 259, 33025, 65535, 259, 33025, 65535,
    };
    unsigned char const rgba[] = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255,
        0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255,
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255,
        0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255,
    };
    char const *stream =
        "\033P7;5q\"1;1;3;7#0;2;0;0;0#259;2;100;0;0#33025;2;0;100;0#65535;"
        "2;0;0;100#256!3~$#3~#32769~#65279~-#256!3@$#3@#32769@#65279@\033"
        "\\";

    (void)argc;
    (void)argv;
    return test_or_planar(stream, expected, rgba, 3, 7, 65536);
}
