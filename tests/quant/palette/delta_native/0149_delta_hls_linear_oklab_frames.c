/* SPDX-License-Identifier: MIT */
/* HLS linear output in oklab workspace retains independent wire colors
 * across paint, keep, change and invalidation. */
#include <sixel.h>
int test_delta_hls_frames(int, int, int);

int
test_delta_native_0149(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_frames(1, 2, 0);
}
