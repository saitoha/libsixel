/* SPDX-License-Identifier: MIT */
/* Retained row kernel: neon short palette sentinel. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_neon_short_palette_sentinel(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(1, 16, 1, 2, 3, 0, 0, 1,
        RETAINED_ROWS_RGB);
}
