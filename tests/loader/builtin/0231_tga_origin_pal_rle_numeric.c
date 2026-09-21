/* SPDX-License-Identifier: MIT */
/* Rebase a repeated PAL8 index once when reading its RLE packet. */
#include "tga_origin_test_common.h"

int
test_loader_0231_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(3);
}
