/* SPDX-License-Identifier: MIT */
/* Fixed native delta: map workers. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0041(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_frames(3, 0, argc > 1 ? argv[1] : NULL);
}
