/* SPDX-License-Identifier: MIT */
/* Verify profile rejected adobe. */
#include "tests/_support/loader/jpeg_cmyk_test_common.h"

int
test_jc_0011(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 2, 3);
}
