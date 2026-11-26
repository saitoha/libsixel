/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int
test_color_0052_abgr_oklab_alpha(int argc, char **argv)
{
    /* Oklab of sRGB blue is about (0.4520, -0.0325, -0.3115).
     * ABGR stores alpha first, then the encoded b, a, and L coordinates. */
    unsigned char const expected[] = {17, 48, 119, 115};
    unsigned char pixel[] = {17, 255, 0, 0};
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    status = sixel_helper_convert_colorspace(pixel, sizeof(pixel),
        SIXEL_PIXELFORMAT_ABGR8888, SIXEL_COLORSPACE_SRGB,
        SIXEL_COLORSPACE_OKLAB);
    return status == SIXEL_OK &&
        memcmp(pixel, expected, sizeof(expected)) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
