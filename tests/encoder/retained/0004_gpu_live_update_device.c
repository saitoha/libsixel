/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0004_gpu_live_update_device(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(3);
}
