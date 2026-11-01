/* SPDX-License-Identifier: MIT */
/* independent matched values and prewrite profile-domain rejection. */
#include "tests/_support/processing/cms/profile_format_common.h"

int
test_cms_0305(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return profile_format_test(17u);
}
