/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_packed_rgbx(int argc, char **argv)
{
    unsigned char pixels[] = {24, 83, 151, 0, 190, 33, 62, 153};

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_packed_roundtrip(argv[1], pixels,
                                SIXEL_PIXELFORMAT_RGBX8888, 0);
}
