/* SPDX-License-Identifier: MIT */
/* Retained row kernel: scalar mixed rgb. */
#include "../../_support/codec/retained_rows_common.h"

int
test_retained_scalar_mixed_rgb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_rows_check(0, 17, 3, 256, 3, 1, 0, 3,
        RETAINED_ROWS_RGB);
}
