/* SPDX-License-Identifier: MIT */
/* Retained row kernel: neon all keep valid. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_neon_all_keep_valid(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(1, 16, 1, 2, 2, 0, 0, 1,
        RETAINED_ROWS_VALID);
}
