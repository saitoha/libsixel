/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0094(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_mpet_legacy_slot_pair_test();
}
