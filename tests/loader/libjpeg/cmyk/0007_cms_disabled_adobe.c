/* SPDX-License-Identifier: MIT */
/* Verify cms disabled adobe. */
#include "tests/loader/jpeg_cmyk_test_common.h"

int
test_jc_0007(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 0, 1);
}
