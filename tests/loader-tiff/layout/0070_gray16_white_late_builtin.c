/* SPDX-License-Identifier: MIT */
/* Gray16 white late builtin with independent values and metadata. */
#include "tests/_support/loader/tiff_managed_test_common.h"

int
test_loader_0283_gray16(int argc, char **argv)
{
    return tiff_managed_failure(argc, argv, 9, 2, 1);
}
