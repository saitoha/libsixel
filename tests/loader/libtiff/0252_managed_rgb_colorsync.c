/* SPDX-License-Identifier: MIT */
/* One independently reported managed float observation. */
#include "tests/loader/tiff_managed_test_common.h"

int
test_loader_0252_mfloat(int argc, char **argv)
{
    return tiff_managed_values(argc, argv, 0, 4, 0);
}
