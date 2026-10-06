/* SPDX-License-Identifier: MIT */
/* HLS kept-color skip uses the retained displayed gray in FS diffusion. */
#include <sixel.h>
int test_delta_hls_residual(int);

int
test_delta_native_0171(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_residual(1);
}
