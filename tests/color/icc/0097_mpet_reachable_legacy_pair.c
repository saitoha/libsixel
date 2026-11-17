/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0097(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_mpet_reachable_legacy_pair_test();
}
