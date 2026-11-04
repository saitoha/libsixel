/* SPDX-License-Identifier: MIT */
/* Consumer rejection seals a partially published image FAILED.
 * Policy: docs/threading/encoder.md
 */
int gpu_rows_apply_test(int);

int
test_gpu_rows_apply_failed(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gpu_rows_apply_test(1);
}
