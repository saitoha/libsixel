/* SPDX-License-Identifier: MIT */
/* Verify builtin hlg16 using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_03(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(35, 0);
}
