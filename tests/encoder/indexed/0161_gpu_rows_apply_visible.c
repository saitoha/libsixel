/* SPDX-License-Identifier: MIT */
/* PaletteApply publishes readable rows before completion.
 * Policy: docs/threading/encoder.md
 */
int gpu_rows_apply_test(int);

int
test_gpu_rows_apply_visible(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gpu_rows_apply_test(0);
}
