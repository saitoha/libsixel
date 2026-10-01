/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: stdin native. */
#include <stddef.h>
int test_structured_animation(int, int, char const *);

int
test_fixed_animation_0013(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_structured_animation(0, 0, "gpl:-");
}
