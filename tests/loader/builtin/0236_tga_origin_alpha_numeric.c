/* SPDX-License-Identifier: MIT */
/* Preserve the zero-based transparent palette key after index rebasing. */
#include "tga_origin_test_common.h"

int
test_loader_0236_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(8);
}
