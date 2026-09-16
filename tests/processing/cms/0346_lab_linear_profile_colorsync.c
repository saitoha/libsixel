/* SPDX-License-Identifier: MIT */
/* One independently reported Lab linear publication observation. */
#include "tests/processing/cms/lab_linear_test_common.h"

int
test_cms_0346_lab_linear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lab_linear_profile_test(4);
}
