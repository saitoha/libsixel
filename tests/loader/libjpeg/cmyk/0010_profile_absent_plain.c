/* SPDX-License-Identifier: MIT */
/* Verify profile absent plain. */
#include "tests/loader/jpeg_cmyk_test_common.h"

int
test_jc_0010(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 0, 2, 2);
}
