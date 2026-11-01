/* SPDX-License-Identifier: MIT */
/* Verify affine plain lcms2. */
#include "tests/_support/loader/jpeg_cmyk_test_common.h"

int
test_jc_0004(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 0, 3, 0);
}
