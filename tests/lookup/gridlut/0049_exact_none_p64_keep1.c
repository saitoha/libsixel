/* SPDX-License-Identifier: MIT */
/* Verify exact none p64 keep1. */
#include <sixel.h>
int test_grid_exact_case(int, int, int);

int
test_lookup_gridlut_0049(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_grid_exact_case(64, 1, 0);
}
