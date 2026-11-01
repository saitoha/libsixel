/* SPDX-License-Identifier: MIT */
/* HLS derived palette cache isolation preserves publication and ownership
 * boundaries. */
#include <sixel.h>
int test_delta_hls_cache(int);

int
test_delta_native_0165(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_cache(0);
}
