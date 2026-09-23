/* SPDX-License-Identifier: MIT */
/* Verify p3 2. */
#include "tests/loader/cg_indexed_test_common.h"

int
test_cgi_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cg_indexed_values(
        "/tests/data/color-numeric/cg-indexed-p3-2.png", 2, 0);
}
