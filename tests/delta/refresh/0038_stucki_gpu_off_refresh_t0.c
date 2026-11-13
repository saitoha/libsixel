/* SPDX-License-Identifier: MIT */
/* Verify stucki gpu off refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0038(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("stucki", "off");
}
