/* SPDX-License-Identifier: MIT */
/* Gray16 black lcms2 with independent values and metadata. */
#include "tests/_support/loader/tiff_managed_test_common.h"

int
test_loader_0281_gray16(int argc, char **argv)
{
    return tiff_managed_values(argc, argv, 8, 3, 0);
}
