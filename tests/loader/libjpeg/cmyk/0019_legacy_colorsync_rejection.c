/* SPDX-License-Identifier: MIT */
/* Verify legacy colorsync rejection. */
#include "tests/loader/jpeg_cmyk_test_common.h"

int
test_jc_0019(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 1, 4, 7);
}
