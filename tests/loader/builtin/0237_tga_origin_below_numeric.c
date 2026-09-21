/* SPDX-License-Identifier: MIT */
/* Map an index below the declared origin to the first stored color. */
#include "tga_origin_test_common.h"

int
test_loader_0237_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(9);
}
