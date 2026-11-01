/* SPDX-License-Identifier: MIT */
/* One independently reported Lab linear publication observation. */
#include "tests/_support/processing/cms/lab_linear_fault_test_common.h"

int
test_cms_0348_lab_linear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cms_lab_failure_probe(CMS_LAB_FAIL_COLORSYNC_LATE);
}
