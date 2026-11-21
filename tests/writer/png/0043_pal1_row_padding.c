/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_pal1_rows(int argc, char **argv)
{
    /* Each final byte contains one sample and seven set padding bits. */
    static unsigned char const indices[] = {
        1, 0, 1, 0, 0, 1, 1, 0, 1,
        0, 1, 0, 1, 1, 0, 0, 1, 0
    };
    unsigned char packed[] = {0xa6, 0xff, 0x59, 0x7f};

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_indexed_roundtrip(argv[1], packed, SIXEL_PIXELFORMAT_PAL1,
                                 9, 2, 2, indices);
}
