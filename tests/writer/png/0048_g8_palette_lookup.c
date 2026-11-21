/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

#include "color_test_common.h"

int
test_png_g8_palette_lookup(int argc, char **argv)
{
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return png_gray_roundtrip(argv[1], 1);
}
