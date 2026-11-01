/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: gray2 p3 fs. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(0, 1, 1, 10);
}
