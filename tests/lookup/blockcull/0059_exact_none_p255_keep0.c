/* SPDX-License-Identifier: MIT */
/* Verify exact none p255 keep0. */
#include <sixel.h>
int test_cull_exact_case(int, int);

int
test_lookup_blockcull_0059(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_cull_exact_case(255, 0);
}
