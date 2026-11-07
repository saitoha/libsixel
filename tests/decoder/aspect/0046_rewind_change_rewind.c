/* SPDX-License-Identifier: MIT */
/* Rewinding after a scale change paints left of the carried x in the same
 * device band. Literal rows independently specify the overwrite order. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0046(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"1;1;2;6" ASPECT_COLORS
        "#1@$#2A\"2;1;2;6#3B$#1C\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 6,
        "RB" "GB" ".B" ".B" "R." "R.", 0U, 0U);
}
