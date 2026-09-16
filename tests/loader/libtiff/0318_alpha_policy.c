/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_alpha_test_common.h"

int
test_loader_0318_alpha(int argc, char **argv)
{
    return tiff_alpha_test(argc, argv, 0, 2, 3);
}
