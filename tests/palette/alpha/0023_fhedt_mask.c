/* SPDX-License-Identifier: MIT */
/* Verify native alpha: fhedt mask. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(0, 15);
}
