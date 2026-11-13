/* SPDX-License-Identifier: MIT */
/* Verify exact none p2 keep0. */
#include <sixel.h>
int test_direct_exact_case(int, int);

int
test_lookup_dispatch_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_exact_case(2, 0);
}
