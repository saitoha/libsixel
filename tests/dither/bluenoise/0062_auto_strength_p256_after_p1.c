/* SPDX-License-Identifier: MIT */
/* Verify auto strength p256 after p1. */
#include <sixel.h>
int test_auto_strength_case(int);

int
test_dither_bluenoise_0062(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_strength_case(6);
}
