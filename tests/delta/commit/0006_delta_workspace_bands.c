/* SPDX-License-Identifier: MIT */
/* Verify delta workspace bands. */
#include <sixel.h>
int test_delta_work_frames(int, int, int);

int
test_delta_native_0096(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_frames(1, 4, 0);
}
