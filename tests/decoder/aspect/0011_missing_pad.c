/* SPDX-License-Identifier: MIT */
/* Verify missing pad with literal output samples. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0011(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"2" ASPECT_COLORS ASPECT_PAINT "\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 2, "R..G",
        0U, 0U);
}
