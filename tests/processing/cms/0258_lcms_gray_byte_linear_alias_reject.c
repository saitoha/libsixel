/* SPDX-License-Identifier: MIT */
/* Valid separate values precede one unsupported LCMS alias condition. */
#include "tests/processing/cms/lcms_expanding_alias_common.h"

int
test_cms_0258(int argc, char **argv)
{
    return lcms_expand_alias(argc, argv, 0, 0);
}
