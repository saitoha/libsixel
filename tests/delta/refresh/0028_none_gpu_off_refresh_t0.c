/* SPDX-License-Identifier: MIT */
/* Verify none gpu off refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("none", "off");
}
