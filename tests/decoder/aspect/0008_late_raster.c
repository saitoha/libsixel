/* SPDX-License-Identifier: MIT */
/* Verify late raster with literal output samples. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0008(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq" ASPECT_COLORS ASPECT_PAINT "\"2;1;2;2#1\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 2, "R..G",
        0U, 0U);
}
