/* SPDX-License-Identifier: MIT */
/* Verify builtin gray byte into analytic gamma RGB with separate buffers. */
#include "tests/processing/cms/builtin_device_common.h"

int
test_cms_0256(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return builtin_device_values(1, 1, 0);
}
