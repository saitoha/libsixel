/* SPDX-License-Identifier: MIT */
/* Rebase source index 255 without overflowing eight-bit PAL8 slots. */
#include "tga_origin_test_common.h"

int
test_loader_0234_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(6);
}
