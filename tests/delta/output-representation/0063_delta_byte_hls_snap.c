/* SPDX-License-Identifier: MIT */
/* Explicit exact snap is reflected in byte HLS candidate comparison. */
#include <sixel.h>
int test_delta_byte_hls_lifetime(int);

int
test_delta_native_0199(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_hls_lifetime(2);
}
