/* SPDX-License-Identifier: MIT */
/* Distinguish this OR composition rule from ordinary painting. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"

int
test_or_dec_0031(int argc, char **argv)
{
    unsigned char expected[1] = { 129 };
    char const *stream =
        "\033P7;5q\"1;1;1;1#0;2;25;50;75#1;2;100;0;0#2;2;0;100;0"
        "#3;2;0;0;100#128@$#1@\033\\";

    (void)argc;
    (void)argv;
    return test_or_decode(stream, expected, 1, 1, 1, 0);
}
