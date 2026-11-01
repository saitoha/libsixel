/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: map workspace. */
#include <stddef.h>
int test_structured_animation(int, int, char const *);

int
test_fixed_animation_0015(int argc, char **argv)
{
    return argc == 2 ? test_structured_animation(11, 0, argv[1]) : 1;
}
