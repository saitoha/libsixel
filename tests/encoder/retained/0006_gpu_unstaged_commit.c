/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0006_gpu_unstaged_commit(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(5);
}
