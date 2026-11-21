/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_pal4_rows(int argc, char **argv)
{
    /* The lower nibble of the last byte on each row is padding. */
    static unsigned char const indices[] = {7, 9, 3, 4, 1, 2};
    unsigned char packed[] = {0x79, 0x3f, 0x41, 0x2f};

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_indexed_roundtrip(argv[1], packed, SIXEL_PIXELFORMAT_PAL4,
                                 3, 2, 10, indices);
}
