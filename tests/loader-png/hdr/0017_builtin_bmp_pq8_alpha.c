/* SPDX-License-Identifier: MIT */
/* Verify embedded BMP pq8 alpha using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_17(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(42, 0);
}
