/* SPDX-License-Identifier: MIT */
/* Verify affine adobe lcms2. */
#include "tests/_support/loader/jpeg_cmyk_test_common.h"

int
test_jc_0003(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 3, 0);
}
