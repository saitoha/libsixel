/* SPDX-License-Identifier: MIT */
/* Fixed native delta: map duplicates. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0035(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_frames(0, 1, argc > 1 ? argv[1] : NULL);
}
