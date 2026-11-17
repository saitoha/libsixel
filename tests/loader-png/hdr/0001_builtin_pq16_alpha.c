/* SPDX-License-Identifier: MIT */
/* Verify builtin pq16 alpha using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_01(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(34, 0);
}
