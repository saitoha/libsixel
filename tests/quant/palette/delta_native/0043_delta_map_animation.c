/* SPDX-License-Identifier: MIT */
/* Fixed native delta: map animation. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0043(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_frames(4, 0, argc > 1 ? argv[1] : NULL);
}
