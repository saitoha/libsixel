/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0024(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_absolute_lut_test(1);
}
