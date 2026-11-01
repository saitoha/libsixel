/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/tiff_device_test_common.h"

int
test_loader_0518_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tiff_device_test(4, 4, 0, 0, 0, 0);
}
