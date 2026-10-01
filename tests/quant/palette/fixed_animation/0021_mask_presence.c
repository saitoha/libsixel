/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: mask presence. */
#include <stddef.h>
int test_fixed_mask_animation(int, int, char const *);

int
test_fixed_animation_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask_animation(3, 7, NULL);
}
