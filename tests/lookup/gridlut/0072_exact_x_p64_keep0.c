/* SPDX-License-Identifier: MIT */
/* Verify exact x p64 keep0. */
#include <sixel.h>
int test_grid_exact_case(int, int, int);

int
test_lookup_gridlut_0072(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_grid_exact_case(64, 0, 2);
}
