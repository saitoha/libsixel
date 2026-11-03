/* SPDX-License-Identifier: MIT */
/* Retained row kernel: neon fullpaint rgb. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_neon_fullpaint_rgb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(1, 16, 1, 256, 0, 0, 0, 1,
        RETAINED_ROWS_RGB);
}
