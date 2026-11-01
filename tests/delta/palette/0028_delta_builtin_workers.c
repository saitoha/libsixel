/* SPDX-License-Identifier: MIT */
/* Fixed native delta: builtin workers. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0040(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_frames(3, 0, NULL);
}
