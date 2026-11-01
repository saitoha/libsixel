/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpu_rectangle(int policy, int method, int observation);

int
test_gpu_rectangle_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_rectangle(SIXEL_LUT_POLICY_EYTZINGER,
        SIXEL_DIFFUSE_NONE, 0);
}
