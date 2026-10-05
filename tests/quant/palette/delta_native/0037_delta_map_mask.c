/* SPDX-License-Identifier: MIT */
/* Fixed native delta: map mask. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0037(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_frames(1, 0, argc > 1 ? argv[1] : NULL);
}
