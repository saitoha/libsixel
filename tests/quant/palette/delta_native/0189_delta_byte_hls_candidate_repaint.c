/* SPDX-License-Identifier: MIT */
/* Byte HLS candidate repaint compares the indexed HLS reconstruction rather
 * than pre-wire RGB. */
#include <sixel.h>
int test_delta_byte_hls_compare(int);

int
test_delta_native_0189(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_hls_compare(1);
}
