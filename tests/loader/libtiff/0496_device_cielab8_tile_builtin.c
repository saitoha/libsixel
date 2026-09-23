/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_device_test_common.h"

int
test_loader_0496_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_device_test(2, 2, 0, 0, 0, 0);
}
