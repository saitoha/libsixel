/* SPDX-License-Identifier: MIT */
/* Verify native alpha: cross space. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0033(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(3, 13);
}
