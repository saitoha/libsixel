/* SPDX-License-Identifier: MIT */
/* Expand raw eight-bit indices relative to a nonzero colormap origin. */
#include "tga_origin_test_common.h"

int
test_loader_0228_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(0);
}
