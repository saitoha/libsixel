/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0052(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_mpet_curve_sample_endpoint_test();
}
