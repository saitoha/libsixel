/* SPDX-License-Identifier: MIT */
/* Changing aspect grows output rows beyond an untrusted raster. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0037(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"1;1;2;3" ASPECT_COLORS "#1@\"2;1;2;3#2A\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 4, "R....G.G",
        0U, SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER);
}
