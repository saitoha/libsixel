/* SPDX-License-Identifier: MIT */
/* Later columns use the new scale; next line advances by that scale. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0030(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"1;1;2;14" ASPECT_COLORS "#1@\"2;1;2;14#2A-#3@\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 14,
        "R....G.G................B.B.",
        0U, 0U);
}
