/* SPDX-License-Identifier: MIT */
/* Verify auto strength reserved65 after p64. */
#include <sixel.h>
int test_auto_strength_case(int);

int
test_dither_bluenoise_0058(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_strength_case(2);
}
