/* SPDX-License-Identifier: MIT */
int retained_gpu_sequence_observe(int);

int
test_retained_0032_gpu_invalidate_then_move(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_sequence_observe(4);
}
