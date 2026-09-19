/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_lab_layout_test_common.h"

int
test_loader_0478_lab_layout(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_lab_layout_test(3, 0);
}
