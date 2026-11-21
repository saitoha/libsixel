/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_pal8_index_255(int argc, char **argv)
{
    /* The last possible index requires all 256 entries. */
    static unsigned char const indices[] = {255, 1, 0};
    unsigned char packed[] = {255, 1, 0};

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_indexed_roundtrip(argv[1], packed, SIXEL_PIXELFORMAT_PAL8,
                                 3, 1, 256, indices);
}
