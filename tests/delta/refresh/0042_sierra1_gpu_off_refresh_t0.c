/* SPDX-License-Identifier: MIT */
/* Verify sierra1 gpu off refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0042(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("sierra:variant=1", "off");
}
