/* SPDX-License-Identifier: MIT */
/* Verify auto strength p2 after p16. */
#include <sixel.h>
int test_auto_strength_case(int);

int
test_dither_bluenoise_0060(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_strength_case(4);
}
