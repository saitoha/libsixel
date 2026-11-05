/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0015_gpu_cpu_migration(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(14);
}
