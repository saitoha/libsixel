/* SPDX-License-Identifier: MIT */
/* Verify delta oklab frames. */
#include <sixel.h>
int test_delta_work_frames(int, int, int);

int
test_delta_native_0082(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_frames(2, 11, 0);
}
