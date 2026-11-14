/* SPDX-License-Identifier: MIT */
int test_fs_gpu_spatial_threshold(int);

int
test_delta_refresh_0098(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fs_gpu_spatial_threshold(0);
}
