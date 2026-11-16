/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0040(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_mpet_count_test(0);
}
