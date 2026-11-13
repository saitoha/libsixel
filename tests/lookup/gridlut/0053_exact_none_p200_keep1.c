/* SPDX-License-Identifier: MIT */
/* Verify exact none p200 keep1. */
#include <sixel.h>
int test_grid_exact_case(int, int, int);

int
test_lookup_gridlut_0053(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_grid_exact_case(200, 1, 0);
}
