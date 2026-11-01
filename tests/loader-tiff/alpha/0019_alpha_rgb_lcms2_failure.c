/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_alpha_test_common.h"

int
test_loader_0323_alpha(int argc, char **argv)
{
    return tiff_alpha_fault(argc, argv, 0, 3, 0);
}
