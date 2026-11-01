/* SPDX-License-Identifier: MIT */
/* Verify native alpha: p3 din99d. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(4, 1);
}
