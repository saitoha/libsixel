/* SPDX-License-Identifier: MIT */
/* Verify burkes gpu auto refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0041(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("burkes", "auto");
}
