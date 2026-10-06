/* SPDX-License-Identifier: MIT */
/* Byte HLS bluenoise captures and compares independent emitted colors
 * without float promotion. */
#include <sixel.h>
int test_delta_byte_hls_method(int);

int
test_delta_native_0187(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_hls_method(SIXEL_DIFFUSE_BLUENOISE_DITHER);
}
