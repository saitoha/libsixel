/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0017_gpu_completion_failure(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(16);
}
