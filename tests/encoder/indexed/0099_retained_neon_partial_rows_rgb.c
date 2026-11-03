/* SPDX-License-Identifier: MIT */
/* Retained row kernel: neon partial rows rgb. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_neon_partial_rows_rgb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(1, 16, 3, 256, 3, 0, 1, 2,
        RETAINED_ROWS_RGB);
}
