/* SPDX-License-Identifier: MIT */
/* Verify fixed animation: stdin precision. */
#include <stddef.h>
int test_structured_animation(int, int, char const *);

int
test_fixed_animation_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_structured_animation(13, 0, "gpl:-");
}
