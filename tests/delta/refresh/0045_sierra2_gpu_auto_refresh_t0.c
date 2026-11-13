/* SPDX-License-Identifier: MIT */
/* Verify sierra2 gpu auto refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0045(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("sierra:variant=2", "auto");
}
