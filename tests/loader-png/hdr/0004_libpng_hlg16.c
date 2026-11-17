/* SPDX-License-Identifier: MIT */
/* Verify libpng hlg16 using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_04(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(35, 1);
}
