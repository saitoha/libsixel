/* SPDX-License-Identifier: MIT */
#include "tests/_support/processing/cms/lab_linear_fault_test_common.h"

int
test_icc_0101(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cms_rgba_legacy_allocation_test(1);
}
