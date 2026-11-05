/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0007_gpu_staged_isolation(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(6);
}
