/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_device_test_common.h"

int
test_loader_0531_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_device_test(14, 2, 1, 0, 0, 0);
}
