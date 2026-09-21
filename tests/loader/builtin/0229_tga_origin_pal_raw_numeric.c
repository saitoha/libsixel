/* SPDX-License-Identifier: MIT */
/* Rebase raw eight-bit file indices to exact zero-based PAL8 slots. */
#include "tga_origin_test_common.h"

int
test_loader_0229_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(1);
}
