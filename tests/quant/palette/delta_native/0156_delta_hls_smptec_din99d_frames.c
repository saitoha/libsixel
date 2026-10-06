/* SPDX-License-Identifier: MIT */
/* HLS smptec output in din99d workspace retains independent wire colors
 * across paint, keep, change and invalidation. */
#include <sixel.h>
int test_delta_hls_frames(int, int, int);

int
test_delta_native_0156(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_frames(2, 4, 0);
}
