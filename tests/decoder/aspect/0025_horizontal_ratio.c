/* SPDX-License-Identifier: MIT */
/* Round 1/2 to 1 output rows per pixel. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0025(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"1;2;2;2" ASPECT_COLORS ASPECT_PAINT "\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 2, "R..G",
        0U, 0U);
}
