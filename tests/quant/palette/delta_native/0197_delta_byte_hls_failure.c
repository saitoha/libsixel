/* SPDX-License-Identifier: MIT */
/* Byte HLS failure clears the borrowed comparison view before reuse. */
#include <sixel.h>
int test_delta_byte_hls_lifetime(int);

int
test_delta_native_0197(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_hls_lifetime(0);
}
