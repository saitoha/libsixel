/* SPDX-License-Identifier: MIT */
/* Use the full sixteen-bit colormap origin for raw source indices. */
#include "tga_origin_test_common.h"

int
test_loader_0232_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(4);
}
