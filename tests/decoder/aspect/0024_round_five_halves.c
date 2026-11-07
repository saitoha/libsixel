/* SPDX-License-Identifier: MIT */
/* Round 5/2 to 3 output rows per pixel. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0024(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"5;2;2;6" ASPECT_COLORS ASPECT_PAINT "\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 6, "R.R.R..G.G.G",
        0U, 0U);
}
