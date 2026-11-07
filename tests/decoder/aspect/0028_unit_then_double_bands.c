/* SPDX-License-Identifier: MIT */
/* Advance by the old scale, then paint with the new scale. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0028(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"1;1;1;8" ASPECT_COLORS "#1@-\"2;1;1;8#2@\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 1, 8, "R.....GG",
        0U, 0U);
}
