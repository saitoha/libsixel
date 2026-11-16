/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0055(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_mpet_float_clut_test();
}
