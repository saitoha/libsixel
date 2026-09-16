/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_alpha_test_common.h"

int
test_loader_0356_alpha16(int argc, char **argv)
{
    return tiff_alpha16_fault(argc, argv, 1, 3, 0);
}
