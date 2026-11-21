/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_packed_abgr(int argc, char **argv)
{
    unsigned char pixels[] = {0, 151, 83, 24, 255, 62, 33, 190};

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_packed_roundtrip(argv[1], pixels,
                                SIXEL_PIXELFORMAT_ABGR8888, 1);
}
