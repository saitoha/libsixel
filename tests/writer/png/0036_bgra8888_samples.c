/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_packed_bgra(int argc, char **argv)
{
    unsigned char pixels[] = {151, 83, 24, 0, 62, 33, 190, 255};

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_packed_roundtrip(argv[1], pixels,
                                SIXEL_PIXELFORMAT_BGRA8888, 1);
}
