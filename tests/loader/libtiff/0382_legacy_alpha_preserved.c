/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_legacy_test_common.h"

int
test_loader_0382_legacy(int argc, char **argv)
{
    return tiff_legacy_test(argc, argv, 2, 1, 6);
}
