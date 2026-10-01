/* SPDX-License-Identifier: MIT */
/* Verify native alpha: hard binning. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(0, 21);
}
