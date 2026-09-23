/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_device_test_common.h"

int
test_loader_0522_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_device_test(12, 4, 0, 0, 0, 0);
}
