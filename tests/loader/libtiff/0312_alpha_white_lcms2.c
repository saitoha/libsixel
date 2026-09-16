/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_alpha_test_common.h"

int
test_loader_0312_alpha(int argc, char **argv)
{
    return tiff_alpha_test(argc, argv, 2, 3, 0);
}
