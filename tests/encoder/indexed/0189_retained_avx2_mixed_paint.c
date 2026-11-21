/* SPDX-License-Identifier: MIT */
/* Retained row kernel: retained avx2 mixed paint. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_avx2_mixed_paint(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(2, 16, 3, 256, 3, 1, 0, 3,
        RETAINED_ROWS_PAINT);
}
