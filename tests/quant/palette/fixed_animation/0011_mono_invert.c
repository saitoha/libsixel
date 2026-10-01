/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: mono invert. */
#include <stddef.h>
int test_mono_animation(int, int, int, int);

int
test_fixed_animation_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_animation(0, 0, 0, 8);
}
