/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_alpha_test_common.h"

int
test_loader_0310_alpha(int argc, char **argv)
{
    return tiff_alpha_test(argc, argv, 1, 4, 0);
}
