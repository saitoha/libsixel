/* SPDX-License-Identifier: MIT */
/* A blank sixel cannot lock the aspect for later paint. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0016(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq?$\"2;1;2;4" ASPECT_COLORS ASPECT_PAINT "\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 4, "R.R..G.G",
        0U, 0U);
}
