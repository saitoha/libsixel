/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: multiframe boundary. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(0, 0, 0, 8);
}
