/* SPDX-License-Identifier: MIT */
/* Verify x dither gpu auto refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0051(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("x_dither", "auto");
}
