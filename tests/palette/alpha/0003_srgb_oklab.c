/* SPDX-License-Identifier: MIT */
/* Verify native alpha: srgb oklab. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(2, 0);
}
