/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_reverse_matrix_test(1, "B2A0");
}
