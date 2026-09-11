/* SPDX-License-Identifier: MIT */
/* Check all final pixels through the public decoder entry point. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"
#include "src/compat_stub.h"

int
test_or_dec_0036(int argc, char **argv)
{
    unsigned char expected[96];
    char const *stream =
        "\033P7;5q\"1;1;8;12#0;2;25;50;75#1;2;100;0;0#2;2;0;100;0"
        "#3;2;0;0;100#1!6~$#2!3~$#1!6~-#2!6~$#1!3~$#2!6~\033\\";
    int x;
    int y;
    int result;

    (void)argc;
    (void)argv;
    for (y = 0; y < 12; y++) {
        for (x = 0; x < 8; x++) {
            expected[y * 8 + x] = x < 3 ? 3 :
                x < 6 ? (y < 6 ? 1 : 2) : 0;
        }
    }
    if (sixel_compat_setenv(
            "_SIXEL_TEST_DECODER_PAINT_THREAD_CREATE_FAILURE", "1")) {
        return EXIT_FAILURE;
    }
    result = test_or_decode(stream, expected, 8, 12, 2, 1);
    (void)sixel_compat_setenv(
        "_SIXEL_TEST_DECODER_PAINT_THREAD_CREATE_FAILURE", "");
    return result;
}
