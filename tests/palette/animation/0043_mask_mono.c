/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: mask mono. */
#include <stddef.h>
int test_fixed_mask_animation(int, int, char const *);

int
test_fixed_animation_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask_animation(1, 15, NULL);
}
