/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpupal_wire(int method, int hls, int lookup);

int
test_gpu_direct_0007_wire_x(int argc, char **argv)
{
    int hls;

    (void)argc;
    (void)argv;
    for (hls = 0; hls < 2; ++hls) {
        if (test_gpupal_wire(SIXEL_DIFFUSE_X_DITHER, hls, SIXEL_LUT_POLICY_NONE)) {
            return 1;
        }
    }
    return 0;
}
