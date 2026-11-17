/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_bpc_20(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_bpc_relative_before_cached_pair_test(1);
}
