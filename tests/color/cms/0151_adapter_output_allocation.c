/* SPDX-License-Identifier: MIT */
#include "tests/_support/processing/cms/lab_linear_fault_test_common.h"

int
test_cms_format_fault_2(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cms_adapter_allocation_test(2);
}
