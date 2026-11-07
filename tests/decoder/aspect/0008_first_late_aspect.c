/* SPDX-License-Identifier: MIT */
/* A late scale preserves old paint and scales subsequent columns. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0008(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq" ASPECT_COLORS "#1@\"2;1;2;4#2A\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 4, "R....G.G",
        0U, 0U);
}
