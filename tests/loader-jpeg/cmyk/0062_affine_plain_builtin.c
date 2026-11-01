/* SPDX-License-Identifier: MIT */
/* Verify affine plain builtin. */
#include "tests/_support/loader/jpeg_cmyk_test_common.h"

int
test_jc_0002(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 0, 2, 0);
}
