/* SPDX-License-Identifier: MIT */
/* Verify auto strength p64 after p256. */
#include <sixel.h>
int test_auto_strength_case(int);

int
test_dither_bluenoise_0057(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_strength_case(1);
}
