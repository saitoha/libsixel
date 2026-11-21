/* SPDX-License-Identifier: MIT */
/* Retained row kernel: retained avx2 backend. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_avx2_backend(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(2, 16, 1, 256, 0, 0, 0, 1,
        RETAINED_ROWS_BACKEND);
}
