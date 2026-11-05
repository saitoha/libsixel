/* SPDX-License-Identifier: MIT */
int retained_gpu_sequence_observe(int);

int
test_retained_0031_gpu_legacy_then_move(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_gpu_sequence_observe(3);
}
