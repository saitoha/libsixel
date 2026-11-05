/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0014_gpu_commit_identity(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(13);
}
