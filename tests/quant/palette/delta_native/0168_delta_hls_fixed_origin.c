/* SPDX-License-Identifier: MIT */
/* HLS fixed palettes preserve all 256 slots and origin state across frames. */
#include <sixel.h>
int test_delta_hls_fixed(int);

int
test_delta_native_0168(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_fixed(2);
}
