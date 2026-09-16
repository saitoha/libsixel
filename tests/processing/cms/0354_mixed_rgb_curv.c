/* SPDX-License-Identifier: MIT */
/* One separately reported mixed-domain observation. */
#include "tests/processing/cms/mixed_domain_common.h"

int
test_cms_0354_mixed(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cms_mixed_values(2);
}
