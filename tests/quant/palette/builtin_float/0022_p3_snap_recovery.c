/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: p3 snap recovery. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(0, 1, 0, 7);
}
