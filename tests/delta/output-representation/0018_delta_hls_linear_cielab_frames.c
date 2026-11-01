/* SPDX-License-Identifier: MIT */
/* HLS linear output in cielab workspace retains independent wire colors
 * across paint, keep, change and invalidation. */
#include <sixel.h>
int test_delta_hls_frames(int, int, int);

int
test_delta_native_0150(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_frames(1, 3, 0);
}
