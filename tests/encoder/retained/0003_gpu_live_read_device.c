/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0003_gpu_live_read_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(2);
}
