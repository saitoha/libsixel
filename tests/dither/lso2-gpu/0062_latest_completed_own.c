/* SPDX-License-Identifier: MIT */
int palette_integration_gpu_test(int);

int
test_lso2_gpu_latest_completed_own(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return palette_integration_gpu_test(1);
}
