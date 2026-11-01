/* SPDX-License-Identifier: MIT */
/* Verify delta p3 linear frames. */
#include <sixel.h>
int test_delta_p3_frames(int, int);

int
test_delta_native_0112(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_frames(1, 0);
}
