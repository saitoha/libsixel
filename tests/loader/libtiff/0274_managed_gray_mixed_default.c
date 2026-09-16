/* SPDX-License-Identifier: MIT */
/* One independently reported managed float observation. */
#include "tests/loader/tiff_managed_test_common.h"

int
test_loader_0274_mfloat(int argc, char **argv)
{
    return tiff_managed_values(argc, argv, 6, 4, 1);
}
