/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: image workspace. */
#include <stddef.h>
int test_map_animation(int, int, char const *);

int
test_fixed_animation_0018(int argc, char **argv)
{
    return argc == 2 ? test_map_animation(9, 0, argv[1]) : 1;
}
