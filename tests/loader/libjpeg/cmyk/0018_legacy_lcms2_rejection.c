/* SPDX-License-Identifier: MIT */
/* Verify legacy lcms2 rejection. */
#include "tests/loader/jpeg_cmyk_test_common.h"

int
test_jc_0018(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 3, 7);
}
