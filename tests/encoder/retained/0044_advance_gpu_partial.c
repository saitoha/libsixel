/* SPDX-License-Identifier: MIT */
int retained_gpu_resolved_observe(int);

int
test_retained_0044_advance_gpu_partial(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_resolved_observe(21);
}
