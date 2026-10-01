/* SPDX-License-Identifier: MIT */
/* Verify native alpha: all transparent. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(0, 5);
}
