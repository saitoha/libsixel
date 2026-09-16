/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_alpha_test_common.h"

int
test_loader_0330_alpha(int argc, char **argv)
{
    return tiff_alpha_fault(argc, argv, 2, 4, 0);
}
