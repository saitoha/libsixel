/* SPDX-License-Identifier: MIT */
/* Verify delta byte map workers. */
#include <stddef.h>
int test_delta_fixed_frames(int, int, char const *);

int
test_delta_native_0053(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (argc != 2) {
        return 1;
    }
    return test_delta_fixed_frames(8, 0, argv[1]);
}
