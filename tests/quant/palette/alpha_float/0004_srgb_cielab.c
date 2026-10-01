/* SPDX-License-Identifier: MIT */
/* Verify native alpha: srgb cielab. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(3, 0);
}
