/* SPDX-License-Identifier: MIT */
/* Verify delta din99d frames. */
#include <sixel.h>
int test_delta_work_frames(int, int, int);

int
test_delta_native_0084(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_frames(4, 0, 1);
}
