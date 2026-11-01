/* SPDX-License-Identifier: MIT */
/* Verify delta byte workers. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0052(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_frames(8, 0, NULL);
}
