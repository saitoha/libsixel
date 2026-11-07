/* SPDX-License-Identifier: MIT */
/* A smaller scale at next-line time can overlap earlier tall paint. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0039(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"2;1;1;12" ASPECT_COLORS "#1~\"1;1;1;12-#2@\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 1, 12, "RRRRRRGRRRRR",
        0U, 0U);
}
