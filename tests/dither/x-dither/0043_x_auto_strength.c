/* SPDX-License-Identifier: MIT */
int test_gpu_arithmetic(int xor_pattern, int mode);

int
test_gpupal_0015_x_auto_strength(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_arithmetic(1, 1);
}
