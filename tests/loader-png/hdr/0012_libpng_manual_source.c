/* SPDX-License-Identifier: MIT */
/* Verify libpng manual source using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_12(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(39, 1);
}
