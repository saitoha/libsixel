/* SPDX-License-Identifier: MIT */
/* Verify profile absent adobe. */
#include "tests/_support/loader/jpeg_cmyk_test_common.h"

int
test_jc_0009(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 2, 2);
}
