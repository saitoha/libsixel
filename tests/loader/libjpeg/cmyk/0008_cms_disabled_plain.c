/* SPDX-License-Identifier: MIT */
/* Verify cms disabled plain. */
#include "tests/loader/jpeg_cmyk_test_common.h"

int
test_jc_0008(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 0, 0, 1);
}
