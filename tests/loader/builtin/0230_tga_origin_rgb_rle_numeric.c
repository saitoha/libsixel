/* SPDX-License-Identifier: MIT */
/* Expand raw and run RLE packets relative to the colormap origin. */
#include "tga_origin_test_common.h"

int
test_loader_0230_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(2);
}
