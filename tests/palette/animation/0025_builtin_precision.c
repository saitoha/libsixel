/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: builtin precision. */
#include <stddef.h>
int test_builtin_animation(int, int, int, int);

int
test_fixed_animation_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_animation(0, 0, 0, 4);
}
