/* SPDX-License-Identifier: MIT */
/* Verify srgb float. */
#include "tests/_support/loader/cg_indexed_test_common.h"

int
test_cgi_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cg_indexed_values(
        "/tests/data/color-numeric/cg-indexed-srgb.png", 8, 4);
}
