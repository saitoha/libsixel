/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_native_fallback_status_test();
}
