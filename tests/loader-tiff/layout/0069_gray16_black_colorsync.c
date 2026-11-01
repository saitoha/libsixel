/* SPDX-License-Identifier: MIT */
/* Gray16 black colorsync with independent values and metadata. */
#include "tests/_support/loader/tiff_managed_test_common.h"

int
test_loader_0282_gray16(int argc, char **argv)
{
    return tiff_managed_values(argc, argv, 8, 4, 0);
}
