/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_packed_xrgb(int argc, char **argv)
{
    unsigned char pixels[] = {0, 24, 83, 151, 153, 190, 33, 62};

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_packed_roundtrip(argv[1], pixels,
                                SIXEL_PIXELFORMAT_XRGB8888, 0);
}
