/* SPDX-License-Identifier: MIT */
/* Retained row kernel: neon palette 256 rgb. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_neon_palette_256_rgb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(1, 256, 1, 256, 0, 0, 0, 1,
        RETAINED_ROWS_RGB);
}
