/* SPDX-License-Identifier: MIT */
/* Verify auto strength p16 after reserved65. */
#include <sixel.h>
int test_auto_strength_case(int);

int
test_dither_bluenoise_0059(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_strength_case(3);
}
