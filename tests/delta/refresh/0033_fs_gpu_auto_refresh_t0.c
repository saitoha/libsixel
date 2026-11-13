/* SPDX-License-Identifier: MIT */
/* Verify fs gpu auto refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0033(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("fs", "auto");
}
