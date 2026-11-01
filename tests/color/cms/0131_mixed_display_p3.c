/* SPDX-License-Identifier: MIT */
/* One separately reported mixed-domain observation. */
#include "tests/_support/processing/cms/mixed_domain_common.h"

int
test_cms_0359_mixed(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cms_mixed_values(7);
}
