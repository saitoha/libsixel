/* SPDX-License-Identifier: MIT */
/* Verify exact none p200 keep1. */
#include <sixel.h>
int test_cull_exact_case(int, int);

int
test_lookup_blockcull_0058(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_cull_exact_case(200, 1);
}
