/* SPDX-License-Identifier: MIT */
/* Decreasing aspect does not move earlier rows or the band top. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0029(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"2;1;1;14" ASPECT_COLORS "#1@-\"1;1;1;14#2@\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 1, 14, "RR..........G.",
        0U, 0U);
}
