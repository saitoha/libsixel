/* SPDX-License-Identifier: MIT */
int retained_gpu_lookup_observe(int);

int
test_retained_0024_gpu_resident_indexes(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_lookup_observe(0);
}
