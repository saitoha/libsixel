/* SPDX-License-Identifier: MIT */
/* Resolve source index 65535 without wrapping the colormap range. */
#include "tga_origin_test_common.h"

int
test_loader_0235_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(7);
}
