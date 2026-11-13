/* SPDX-License-Identifier: MIT */
/* Verify exact none p200 keep1. */
#include <sixel.h>
int test_direct_exact_case(int, int);

int
test_lookup_dispatch_0040(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_exact_case(200, 1);
}
