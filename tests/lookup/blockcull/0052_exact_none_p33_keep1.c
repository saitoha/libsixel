/* SPDX-License-Identifier: MIT */
/* Verify exact none p33 keep1. */
#include <sixel.h>
int test_cull_exact_case(int, int);

int
test_lookup_blockcull_0052(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_cull_exact_case(33, 1);
}
