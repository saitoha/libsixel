/* SPDX-License-Identifier: MIT */
int retained_gpu_lookup_observe(int);

int
test_retained_0027_gpu_resident_valid_upload(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_lookup_observe(3);
}
