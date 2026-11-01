/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_alpha_test_common.h"

int
test_loader_0309_alpha(int argc, char **argv)
{
    return tiff_alpha_test(argc, argv, 1, 3, 0);
}
