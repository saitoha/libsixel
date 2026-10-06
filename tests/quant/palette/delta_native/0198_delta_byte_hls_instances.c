/* SPDX-License-Identifier: MIT */
/* Independent byte HLS contexts rebuild comparison views after palette
 * changes. */
#include <sixel.h>
int test_delta_byte_hls_lifetime(int);

int
test_delta_native_0198(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_hls_lifetime(1);
}
