/* SPDX-License-Identifier: MIT */
/* OR combines multiplied pixels in device rows and keeps zero opaque. */
#include <sixel.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0038(int argc, char **argv)
{
    static char const stream[] =
        "\033P0;5q\"1;1;2;4" ASPECT_COLORS "#0;2;0;0;0#1@$\"2;1;2;4#2@\033\\";

    (void)argc;
    (void)argv;
    return test_decoder_aspect_pixels(stream, 2, 4, "BKGKKKKK",
        0U, 0U);
}
