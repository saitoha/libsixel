/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: mask full table. */
#include <stddef.h>
int test_fixed_mask_animation(int, int, char const *);

int
test_fixed_animation_0020(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask_animation(0, 2, NULL);
}
