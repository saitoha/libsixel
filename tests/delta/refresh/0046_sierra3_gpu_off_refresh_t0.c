/* SPDX-License-Identifier: MIT */
/* Verify sierra3 gpu off refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0046(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("sierra:variant=3", "off");
}
