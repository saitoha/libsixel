/* SPDX-License-Identifier: MIT */
int test_gpu_arithmetic(int xor_pattern, int mode);

int
test_gpupal_0014_a_auto_strength(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_arithmetic(0, 1);
}
