/* SPDX-License-Identifier: MIT */
/* One independently reported managed float observation. */
#include "tests/_support/loader/tiff_managed_test_common.h"

int
test_loader_0272_mfloat(int argc, char **argv)
{
    return tiff_managed_failure(argc, argv, 2, 2, 4);
}
