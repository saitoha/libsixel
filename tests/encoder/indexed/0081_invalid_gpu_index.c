/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/execution-map/README.md
 */
int injected_gpu_program_test(int);

int
test_invalid_gpu_index(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return injected_gpu_program_test(1);
}
