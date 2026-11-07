/* SPDX-License-Identifier: MIT */
/* Head detection includes blank/repeat/cursor tokens and excludes colors,
 * DCS macro geometry and bytes after the first packet terminator. */
#include <stdlib.h>
#include <string.h>
#include "src/fromsixel-internal.h"

int
test_decoder_aspect_0042(int argc, char **argv)
{
    static char const *const streams[] = {
        "\033P9q#1;2;100;0;0\"5;2@\"3;1@\033\\",
        "\033Pq@\"1;2@\"5;4@\033\\",
        "\033Pq@\033\\\033Pq\"2;1@\033\\",
        "\033Pq\"2;1@\033\\\033Pq\"3;1@\033\\",
        "\033Pq\"2;1@\"3;1\033\\",
        "\033Pq@\"2;1@\033\\",
        "\033Pq?\"2;1@\033\\",
        "\033Pq!1\"2;1@\033\\",
        "\033Pq$\"2;1@\033\\",
        "\033Pq-\"2;1@\033\\",
        "\033Pq\"2;1@\"1;1@\033\\"
    };
    size_t i;
    int expected;

    (void)argc;
    (void)argv;
    if (sixel_aspect_scan(NULL, 0, 0) != 0 ||
            sixel_aspect_scan((unsigned char const *)"", -1, 0) != 0) {
        return EXIT_FAILURE;
    }
    for (i = 0; i < sizeof(streams) / sizeof(streams[0]); ++i) {
        expected = i >= 5;
        if (sixel_aspect_scan((unsigned char const *)streams[i],
                (int)strlen(streams[i]), 0) != expected) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
