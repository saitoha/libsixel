/* SPDX-License-Identifier: MIT */
/* Retained row kernel: neon mixed valid. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_neon_mixed_valid(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(1, 16, 3, 256, 3, 1, 0, 3,
        RETAINED_ROWS_VALID);
}
