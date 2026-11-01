/* SPDX-License-Identifier: MIT */
/* Verify builtin gray byte into analytic linear RGB with alias buffers. */
#include "tests/_support/processing/cms/builtin_device_common.h"

int
test_cms_0255(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return builtin_device_values(1, 0, 1);
}
