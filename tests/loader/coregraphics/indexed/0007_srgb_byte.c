/* SPDX-License-Identifier: MIT */
/* Verify srgb byte. */
#include "tests/loader/cg_indexed_test_common.h"

int
test_cgi_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cg_indexed_values(
        "/tests/data/color-numeric/cg-indexed-srgb.png", 8, 3);
}
