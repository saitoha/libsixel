/* SPDX-License-Identifier: MIT */
/* Verify builtin cms disabled using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_15(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(41, 0);
}
