/* SPDX-License-Identifier: MIT */
/* Verify fractional ratio with literal output samples. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0004(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"3;2;2;2" ASPECT_COLORS ASPECT_PAINT "\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 2, "R..G",
        0U, 0U);
}
