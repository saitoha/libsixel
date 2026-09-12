/*
 * SPDX-License-Identifier: MIT
 * Verify the byte packing of the independently calculated DIN99d blue:
 * L/100 maps to [0,255]; a/100 and b/100 map [-1,1] to [0,255].
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int
test_color_0003_din99d_byte(int argc, char **argv)
{
    static unsigned char const expected[] = {91, 142, 69};
    unsigned char pixels[] = {0, 0, 255};
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    status = sixel_helper_convert_colorspace(pixels, sizeof(pixels),
        SIXEL_PIXELFORMAT_RGB888,
        SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_DIN99D);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    return memcmp(pixels, expected, sizeof(expected)) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
