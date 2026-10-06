/* SPDX-License-Identifier: MIT */
/* RGB and HLS syntax changes preserve compatible history and compare their
 * distinct emitted colors. */
#include <sixel.h>
int test_delta_hls_reuse(void);

int
test_delta_native_0172(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_reuse();
}
