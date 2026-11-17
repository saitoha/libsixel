/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0078(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_colorspace_lab_fallback_test(1);
}
