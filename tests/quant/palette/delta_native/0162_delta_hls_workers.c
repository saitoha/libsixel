/* SPDX-License-Identifier: MIT */
/* HLS P3 delta preserves emitted colors through actual worker dispatch. */
#include <sixel.h>
int test_delta_hls_frames(int, int, int);

int
test_delta_native_0162(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_frames(3, 2, 1);
}
