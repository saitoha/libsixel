/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: linear output. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(1, 2, 0, 0);
}
