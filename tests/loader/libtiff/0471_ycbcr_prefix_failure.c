/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_ycbcr_test_common.h"

int
test_loader_0471_ycbcr(int argc, char **argv)
{
    return tiff_ycbcr_test(argc, argv, 2, 1);
}
