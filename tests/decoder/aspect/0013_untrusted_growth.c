/* SPDX-License-Identifier: MIT */
/* Verify untrusted growth with literal output samples. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0013(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"2;1;2;3" ASPECT_COLORS ASPECT_PAINT "$#3?C\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 6, "R.R..G.G.B.B",
        0U, SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER);
}
