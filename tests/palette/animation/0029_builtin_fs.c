/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: builtin fs. */
#include <stddef.h>
int test_builtin_animation(int, int, int, int);

int
test_fixed_animation_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_animation(0, 0, 3, 11);
}
