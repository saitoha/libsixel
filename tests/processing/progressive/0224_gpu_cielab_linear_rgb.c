/* SPDX-License-Identifier: MIT */
int test_progressive_gpu_colors_case(int workspace, int output_space, int hls);

int
test_progressive_0224(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_progressive_gpu_colors_case(3, 1, 0);
}
