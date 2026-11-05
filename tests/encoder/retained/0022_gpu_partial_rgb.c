/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0022_gpu_partial_rgb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(21);
}
