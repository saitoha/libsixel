/* SPDX-License-Identifier: MIT */
/* One independently reported Lab linear publication observation. */
#include "tests/_support/processing/cms/lab_linear_fault_test_common.h"

int
test_loader_0191_lab_linear(int argc, char **argv)
{
    return tiff_lab_failure_probe(argc, argv, CMS_LAB_FAIL_COLORSYNC_LATE);
}
