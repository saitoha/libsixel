/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_layout_test_common.h"

int
test_loader_0458_layout(int argc, char **argv)
{
    int result;

    if (argc != 2) { return 2; }
    result = test_tiff_layout(argv[1], 20, 2, 6, 0);
    if (result != 0) { return result; }
    return 0;
}
