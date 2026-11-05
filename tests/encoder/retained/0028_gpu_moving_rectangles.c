/* SPDX-License-Identifier: MIT */
int retained_gpu_sequence_observe(int);

int
test_retained_0028_gpu_moving_rectangles(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_sequence_observe(0);
}
