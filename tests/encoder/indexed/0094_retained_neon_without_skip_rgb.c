/* SPDX-License-Identifier: MIT */
/* Retained row kernel: neon without skip rgb. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_neon_without_skip_rgb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(1, 16, 1, 256, 4, 0, 0, 1,
        RETAINED_ROWS_RGB);
}
