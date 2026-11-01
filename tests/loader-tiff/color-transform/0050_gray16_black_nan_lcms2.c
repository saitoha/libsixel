/* SPDX-License-Identifier: MIT */
/* Gray16 black nan lcms2 with independent values and metadata. */
#include "tests/_support/loader/tiff_managed_test_common.h"

int
test_loader_0284_gray16(int argc, char **argv)
{
    return tiff_managed_failure(argc, argv, 8, 3, 3);
}
