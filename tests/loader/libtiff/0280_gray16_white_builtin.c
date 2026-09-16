/* SPDX-License-Identifier: MIT */
/* Gray16 white builtin with independent values and metadata. */
#include "tests/loader/tiff_managed_test_common.h"

int
test_loader_0280_gray16(int argc, char **argv)
{
    return tiff_managed_values(argc, argv, 9, 2, 0);
}
