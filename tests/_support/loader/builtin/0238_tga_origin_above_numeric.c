/* SPDX-License-Identifier: MIT */
/* Map an index above the declared palette range to its first color. */
#include "tga_origin_test_common.h"

int
test_loader_0238_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(10);
}
