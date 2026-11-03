/* SPDX-License-Identifier: MIT */
/* Retained row kernel: neon tail rgb. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_neon_tail_rgb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(1, 17, 3, 256, 3, 0, 0, 3,
        RETAINED_ROWS_RGB);
}
