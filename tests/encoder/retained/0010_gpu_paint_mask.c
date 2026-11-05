/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0010_gpu_paint_mask(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(9);
}
