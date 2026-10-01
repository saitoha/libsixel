/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: builtin p3. */
#include <stddef.h>
int test_builtin_animation(int, int, int, int);

int
test_fixed_animation_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_animation(1, 1, 0, 0);
}
