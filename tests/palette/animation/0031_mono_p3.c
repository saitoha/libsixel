/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: mono p3. */
#include <stddef.h>
int test_mono_animation(int, int, int, int);

int
test_fixed_animation_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_animation(1, 1, 1, 0);
}
