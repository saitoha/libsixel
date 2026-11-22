/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int
test_color_0053_bgr_smptec_to_srgb(int argc, char **argv)
{
    /* Deriving the D65 matrices from the two sets of chromaticities
     * gives linear RGB bytes (16, 89, 2) before the sRGB transfer. */
    unsigned char const expected[] = {22, 159, 71};
    unsigned char pixel[] = {32, 160, 64};
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    status = sixel_helper_convert_colorspace(pixel, sizeof(pixel),
        SIXEL_PIXELFORMAT_BGR888, SIXEL_COLORSPACE_SMPTEC,
        SIXEL_COLORSPACE_GAMMA);
    return status == SIXEL_OK &&
        memcmp(pixel, expected, sizeof(expected)) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
