/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: mono oklab. */
#include <stddef.h>
int test_mono_animation(int, int, int, int);

int
test_fixed_animation_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_animation(2, 0, 0, 0);
}
