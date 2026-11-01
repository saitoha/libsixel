/* SPDX-License-Identifier: MIT */
/* Verify legacy default values. */
#include "tests/_support/loader/jpeg_cmyk_test_common.h"

int
test_jc_0016(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 2, 5);
}
