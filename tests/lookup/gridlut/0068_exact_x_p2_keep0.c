/* SPDX-License-Identifier: MIT */
/* Verify exact x p2 keep0. */
#include <sixel.h>
int test_grid_exact_case(int, int, int);

int
test_lookup_gridlut_0068(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_grid_exact_case(2, 0, 2);
}
