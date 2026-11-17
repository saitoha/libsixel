/* SPDX-License-Identifier: MIT */
/* Verify libpng pq no tonemap using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_10(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(38, 1);
}
