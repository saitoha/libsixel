/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0134(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_nonrelative_mpet_palette_test(1);
}
