/* SPDX-License-Identifier: MIT */
/* Verify native alpha: p3 linear. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(1, 1);
}
