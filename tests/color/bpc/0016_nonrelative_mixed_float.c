/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_bpc_15(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_mpet_legacy_mixed_precedence_test(0, 1);
}
