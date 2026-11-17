/* SPDX-License-Identifier: MIT */
/* Verify builtin pq no tonemap using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_09(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(38, 0);
}
