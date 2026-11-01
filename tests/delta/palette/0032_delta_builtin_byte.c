/* SPDX-License-Identifier: MIT */
/* Fixed native delta: builtin byte. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0044(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_frames(5, 0, NULL);
}
