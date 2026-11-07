/* SPDX-License-Identifier: MIT */
/* Round 7/2 to 4 output rows per pixel. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0027(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"7;2;2;8" ASPECT_COLORS ASPECT_PAINT "\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 8, "R.R.R.R..G.G.G.G",
        0U, 0U);
}
