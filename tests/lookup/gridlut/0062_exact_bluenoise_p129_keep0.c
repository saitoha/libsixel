/* SPDX-License-Identifier: MIT */
/* Verify exact bluenoise p129 keep0. */
#include <sixel.h>
int test_grid_exact_case(int, int, int);

int
test_lookup_gridlut_0062(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_grid_exact_case(129, 0, 1);
}
