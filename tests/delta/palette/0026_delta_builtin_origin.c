/* SPDX-License-Identifier: MIT */
/* Fixed native delta: builtin origin. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0038(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_frames(2, 0, NULL);
}
