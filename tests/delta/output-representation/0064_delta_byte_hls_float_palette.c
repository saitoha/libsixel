/* SPDX-License-Identifier: MIT */
/* Legacy indexed HLS history follows the final float palette rather than
 * its byte shadow. */
#include <sixel.h>
int test_delta_byte_hls_float_palette(int);

int
test_delta_native_0200(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_hls_float_palette(0);
}
