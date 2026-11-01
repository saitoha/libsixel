/* SPDX-License-Identifier: MIT */
/* Verify p3 4. */
#include "tests/_support/loader/cg_indexed_test_common.h"

int
test_cgi_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cg_indexed_values(
        "/tests/data/color-numeric/cg-indexed-p3-4.png", 4, 0);
}
