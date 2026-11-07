/* SPDX-License-Identifier: MIT */
/* A band advance remains six source rows and becomes twelve output rows. */
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0017(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"2;1;2;14" ASPECT_COLORS "#1~?-#2?@\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 14,
        "R.R.R.R.R.R.R.R.R.R.R.R..G.G", 0U, 0U);
}
