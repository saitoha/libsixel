/* SPDX-License-Identifier: MIT */
/* Verify builtin hlg display parameters using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_13(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(40, 0);
}
