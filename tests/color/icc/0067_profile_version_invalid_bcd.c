/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0067(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_profile_header_test(2);
}
