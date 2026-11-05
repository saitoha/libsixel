/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0018_gpu_invalidate(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(17);
}
