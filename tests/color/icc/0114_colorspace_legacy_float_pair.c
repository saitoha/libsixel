/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0114(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_colorspace_legacy_float_pair_test(0);
}
