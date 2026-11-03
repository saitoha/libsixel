/* SPDX-License-Identifier: MIT */
/* Retained row kernel: subvector scalar backend. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_subvector_scalar_backend(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(-1, 15, 1, 256, 0, 0, 0, 1,
        RETAINED_ROWS_BACKEND);
}
