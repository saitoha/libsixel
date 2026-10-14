/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpupal_wire(int method, int hls, int lookup);

int
test_gpupal_0018_wire_a(int argc, char **argv)
{
    int hls, lookup;

    (void)argc;
    (void)argv;
    for (hls = 0; hls < 2; ++hls) {
        for (lookup = 0; lookup < 2; ++lookup) {
            if (test_gpupal_wire(SIXEL_DIFFUSE_A_DITHER, hls,
                    lookup ? SIXEL_LUT_POLICY_EYTZINGER
                           : SIXEL_LUT_POLICY_NONE)) {
                return 1;
            }
        }
    }
    return 0;
}
