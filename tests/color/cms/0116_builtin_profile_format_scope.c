/* SPDX-License-Identifier: MIT */
/* known-profile rejection trace and excluded source/destination controls. */
#include "tests/_support/processing/cms/profile_format_scope_common.h"

int
test_cms_0342(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return profile_format_scope(2);
}
