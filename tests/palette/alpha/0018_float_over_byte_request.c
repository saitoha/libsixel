/* SPDX-License-Identifier: MIT */
/* Verify native alpha: float over byte request. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0018(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(0, 9);
}
