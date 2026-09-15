/* SPDX-License-Identifier: MIT */
/* Verify builtin CMYK float into analytic linear RGB with separate buffers. */
#include "tests/processing/cms/builtin_device_common.h"

int
test_cms_0250(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return builtin_device_values(0, 0, 0);
}
