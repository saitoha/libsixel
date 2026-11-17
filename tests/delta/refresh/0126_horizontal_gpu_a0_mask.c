/* SPDX-License-Identifier: MIT */
int test_refresh_horizontal_gpu_mask(unsigned int);

int
test_delta_refresh_0126(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_horizontal_gpu_mask(0);
}
