/* SPDX-License-Identifier: MIT */
/* Verify affine adobe colorsync. */
#include "tests/loader/jpeg_cmyk_test_common.h"

int
test_jc_0005(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 4, 0);
}
