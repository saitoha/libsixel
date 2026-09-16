/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_alpha_test_common.h"

int
test_loader_0344_alpha16(int argc, char **argv)
{
    return tiff_alpha16_test(argc, argv, 0, 2, 1);
}
