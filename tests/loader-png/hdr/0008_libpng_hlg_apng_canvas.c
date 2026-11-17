/* SPDX-License-Identifier: MIT */
/* Verify libpng hlg apng canvas using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_08(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(37, 1);
}
