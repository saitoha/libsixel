/* SPDX-License-Identifier: MIT */
/* Verify native alpha: vptree reserved. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0030(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(0, 23);
}
