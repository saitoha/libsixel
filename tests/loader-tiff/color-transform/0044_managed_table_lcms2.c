/* SPDX-License-Identifier: MIT */
/* One independently reported managed float observation. */
#include "tests/_support/loader/tiff_managed_test_common.h"

int
test_loader_0262_mfloat(int argc, char **argv)
{
    return tiff_managed_values(argc, argv, 5, 3, 0);
}
