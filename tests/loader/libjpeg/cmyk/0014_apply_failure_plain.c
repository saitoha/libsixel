/* SPDX-License-Identifier: MIT */
/* Verify apply failure plain. */
#include "tests/loader/jpeg_cmyk_test_common.h"

int
test_jc_0014(int argc, char **argv)
{
    return jpeg_cmyk_values(argc, argv, 0, 2, 4);
}
