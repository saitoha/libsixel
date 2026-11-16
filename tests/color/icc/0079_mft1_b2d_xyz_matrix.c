/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0079(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_reverse_matrix_test(0, "B2D0");
}
