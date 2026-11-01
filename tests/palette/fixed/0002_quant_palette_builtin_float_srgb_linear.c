/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: srgb linear. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(1, 0, 0, 0);
}
