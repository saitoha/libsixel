/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_pal2_rows(int argc, char **argv)
{
    /* Index 3 appears only in padding. PLTE ends at index 2. */
    static unsigned char const indices[] = {0, 2, 0, 1, 2, 2, 1, 0, 2, 1};
    unsigned char packed[] = {0x21, 0xbf, 0x92, 0x7f};

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_indexed_roundtrip(argv[1], packed, SIXEL_PIXELFORMAT_PAL2,
                                 5, 2, 3, indices);
}
