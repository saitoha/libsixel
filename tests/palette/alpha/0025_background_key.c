/* SPDX-License-Identifier: MIT */
/* Verify native alpha: background key. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0025(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(0, 17);
}
