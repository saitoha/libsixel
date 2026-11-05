/* SPDX-License-Identifier: MIT */
int retained_gpu_lookup_observe(int);

int
test_retained_0026_gpu_resident_rgb_upload(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_lookup_observe(2);
}
