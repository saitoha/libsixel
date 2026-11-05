/* SPDX-License-Identifier: MIT */
int retained_gpu_observe(int);

int
test_retained_0005_gpu_stage_building(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_observe(4);
}
