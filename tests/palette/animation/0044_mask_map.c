/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: mask map. */
#include <stddef.h>
int test_fixed_mask_animation(int, int, char const *);

int
test_fixed_animation_0023(int argc, char **argv)
{
    return argc == 2 ? test_fixed_mask_animation(0, 7, argv[1]) : 1;
}
