/* SPDX-License-Identifier: MIT */
/* Verify builtin CMYK float into analytic gamma RGB with alias buffers. */
#include "tests/_support/processing/cms/builtin_device_common.h"

int
test_cms_0253(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return builtin_device_values(0, 1, 1);
}
