/* SPDX-License-Identifier: MIT */
/* Byte HLS sierra3 captures and compares independent emitted colors without
 * float promotion. */
#include <sixel.h>
int test_delta_byte_hls_method(int);

int
test_delta_native_0183(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_hls_method(SIXEL_DIFFUSE_SIERRA3);
}
