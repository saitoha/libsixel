/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_alpha_test_common.h"

int
test_loader_0345_alpha16(int argc, char **argv)
{
    return tiff_alpha16_test(argc, argv, 1, 2, 1);
}
