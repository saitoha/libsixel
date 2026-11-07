/* SPDX-License-Identifier: MIT */
/* Apply the last head scale to subsequent paint. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0007(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"2;1;2;4\"3;1;2;6" ASPECT_COLORS ASPECT_PAINT "\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 6, "R.R.R..G.G.G",
        0U, 0U);
}
