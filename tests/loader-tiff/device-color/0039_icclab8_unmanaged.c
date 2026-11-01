/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_lab_layout_test_common.h"

int
test_loader_0481_lab_layout(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_lab_layout_test(0, 1);
}
