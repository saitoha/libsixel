/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: vt340 duplicates. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(1, 0, 2, 0);
}
