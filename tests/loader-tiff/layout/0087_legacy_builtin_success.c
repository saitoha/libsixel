/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_legacy_test_common.h"

int
test_loader_0366_legacy(int argc, char **argv)
{
    return tiff_legacy_test(argc, argv, 2, 0, 0);
}
