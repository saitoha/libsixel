/* SPDX-License-Identifier: MIT */
/* Verify libpng pq16 alpha using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_02(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(34, 1);
}
