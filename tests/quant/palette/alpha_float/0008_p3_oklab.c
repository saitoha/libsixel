/* SPDX-License-Identifier: MIT */
/* Verify native alpha: p3 oklab. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(2, 1);
}
