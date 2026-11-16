/* SPDX-License-Identifier: MIT */
#include "tests/_support/diagnostics/icc_numeric_common.h"

int
test_icc_0092(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return icc_numeric_colorspace_lab_slot_zero_test(1);
}
