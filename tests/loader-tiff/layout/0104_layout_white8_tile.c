/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_layout_test_common.h"

int
test_loader_0417_layout(int argc, char **argv)
{
    int result;

    if (argc != 2) { return 2; }
    result = test_tiff_layout(argv[1], 4, 0, 0, 0);
    if (result != 0) { return result; }
    result = test_tiff_layout(argv[1], 4, 2, 0, 0);
    if (result != 0) { return result; }
    return 0;
}
