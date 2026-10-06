/* SPDX-License-Identifier: MIT */
/* Verify delta p3 workers. */
#include <sixel.h>
int test_delta_p3_frames(int, int);

int
test_delta_native_0116(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_frames(2, 1);
}
