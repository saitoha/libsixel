/* SPDX-License-Identifier: MIT */
/* Equal rounded scales keep the fast path and the single-raster samples. */
#include <stdlib.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0031(int argc, char **argv)
{
    static char const single[] =
        "\033Pq" ASPECT_COLORS "\"3;1;2;6#1@#2A\033\\";
    static char const repeated[] =
        "\033Pq" ASPECT_COLORS "\"5;2;2;6#1@\"3;1;2;6#2A\033\\";

    (void)argc;
    (void)argv;
    if (test_decoder_aspect_pixels(single, 2, 6, "R.R.R..G.G.G",
                                   0U, 0U) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return test_decoder_aspect_pixels(repeated, 2, 6, "R.R.R..G.G.G",
                                      0U, 0U);
}
