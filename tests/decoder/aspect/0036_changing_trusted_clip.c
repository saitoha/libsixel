/* SPDX-License-Identifier: MIT */
/* Changing aspect clips output rows and records the clipped paint. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0036(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"1;1;2;3" ASPECT_COLORS "#1@\"2;1;2;3#2A\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 3, "R....G",
        SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE,
        SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER |
        SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER);
}
