/* SPDX-License-Identifier: MIT */
/* Verify lso2 raster gpu off refresh t0. */
#include <sixel.h>
int test_refresh_dither_equivalence(char const *, char const *);

int
test_delta_refresh_0056(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_dither_equivalence("lso2:scan=raster", "off");
}
