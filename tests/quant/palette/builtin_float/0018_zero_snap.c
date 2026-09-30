/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: zero snap. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0018(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(0, 1, 0, 3);
}
