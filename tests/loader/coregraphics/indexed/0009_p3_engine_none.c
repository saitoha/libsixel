/* SPDX-License-Identifier: MIT */
/* Verify p3 engine none. */
#include "tests/loader/cg_indexed_test_common.h"

int
test_cgi_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cg_indexed_values(
        "/tests/data/color-numeric/cg-indexed-p3-8.png", 8, 5);
}
