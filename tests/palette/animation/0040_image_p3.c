/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: image p3. */
#include <stddef.h>
int test_map_animation(int, int, char const *);

int
test_fixed_animation_0019(int argc, char **argv)
{
    return argc == 2 ? test_map_animation(5, 1, argv[1]) : 1;
}
