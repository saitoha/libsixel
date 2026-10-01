/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: builtin gamma. */
#include <stddef.h>
int test_builtin_animation(int, int, int, int);

int
test_fixed_animation_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_animation(0, 0, 0, 0);
}
