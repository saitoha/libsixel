/* SPDX-License-Identifier: MIT */
/* A later invalid index cannot invalidate a completed prefix.
 * Policy: docs/threading/encoder.md
 */
int injected_gpu_program_test(int);

int
test_gpu_rows_invalid_later(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return injected_gpu_program_test(2);
}
