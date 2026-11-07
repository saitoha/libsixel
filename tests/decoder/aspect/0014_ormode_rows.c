/* SPDX-License-Identifier: MIT */
/* Verify ormode rows with literal output samples. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0014(int argc, char **argv)
{
    static char const stream[] =
        "\033P0;5q\"2;1;2;4#0;2;0;0;0" ASPECT_COLORS ASPECT_PAINT "\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 4, "RKRKKGKG",
        0U, 0U);
}
