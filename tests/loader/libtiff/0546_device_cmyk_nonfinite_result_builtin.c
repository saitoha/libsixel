/* SPDX-License-Identifier: MIT */
#include "tests/loader/tiff_device_test_common.h"

int
test_loader_0546_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_device_test(6, 2, 0, 7, 0, 0);
}
