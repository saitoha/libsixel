/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_device_test_common.h"

int
test_loader_0519_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_device_test(5, 4, 0, 0, 0, 0);
}
