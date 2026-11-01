/* SPDX-License-Identifier: MIT */
/* Verify original Gray8 samples and the fixed RGB float target. */
#include "tests/_support/processing/cms/colorsync_gray8_common.h"

int
test_cms_0273(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return colorsync_gray8(1, 1);
}
