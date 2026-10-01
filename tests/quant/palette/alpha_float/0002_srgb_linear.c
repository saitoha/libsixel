/* SPDX-License-Identifier: MIT */
/* Verify native alpha: srgb linear. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(1, 0);
}
