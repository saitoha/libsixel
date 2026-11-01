/* SPDX-License-Identifier: MIT */
/* Verify affine adobe builtin. */
#include "tests/_support/loader/jpeg_cmyk_test_common.h"

int
test_jc_0001(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 2, 0);
}
