/* SPDX-License-Identifier: MIT */
/* Retained row kernel: retained avx2 all keep rgb. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_avx2_all_keep_rgb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(2, 16, 1, 2, 2, 0, 0, 1,
        RETAINED_ROWS_RGB);
}
