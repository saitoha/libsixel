/* SPDX-License-Identifier: MIT */
/* Verify native alpha: byte eligible. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(0, 7);
}
