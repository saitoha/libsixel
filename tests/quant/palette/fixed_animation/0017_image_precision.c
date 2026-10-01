/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: image precision. */
#include <stddef.h>
int test_map_animation(int, int, char const *);

int
test_fixed_animation_0017(int argc, char **argv)
{
    return argc == 2 ? test_map_animation(0, 0, argv[1]) : 1;
}
