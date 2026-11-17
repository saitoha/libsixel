/* SPDX-License-Identifier: MIT */
/* Verify builtin manual source using independent samples. */
#include "tests/_support/loader/png_cicp_test_common.h"

int
test_png_hdr_11(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_cicp_test(39, 0);
}
