/* SPDX-License-Identifier: MIT */
/* Verify apply failure adobe. */
#include "tests/loader/jpeg_cmyk_test_common.h"

int
test_jc_0013(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 2, 4);
}
