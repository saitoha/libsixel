/* SPDX-License-Identifier: MIT */
/* Keep valid UNKNOWN domains and reject only mismatched adapter layouts. */
#include "tests/_support/processing/cms/colorsync_channel_scope_common.h"

int
test_cms_0286(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return colorsync_channel_scope(0);
}
