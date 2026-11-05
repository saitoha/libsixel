/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0002_gpu_same_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(1);
}
