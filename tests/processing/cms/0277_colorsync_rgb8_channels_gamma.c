/* SPDX-License-Identifier: MIT */
/* Check native channel counts without changing accepted numeric values. */
#include "tests/processing/cms/colorsync_channels_common.h"

int
test_cms_0277(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return colorsync_channels(1, 1);
}
