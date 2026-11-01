/* SPDX-License-Identifier: MIT */
/* Verify affine plain colorsync. */
#include "tests/_support/loader/jpeg_cmyk_test_common.h"

int
test_jc_0006(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 0, 4, 0);
}
