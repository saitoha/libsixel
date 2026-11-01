/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_device_test_common.h"

int
test_loader_0498_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_device_test(4, 2, 0, 0, 0, 0);
}
