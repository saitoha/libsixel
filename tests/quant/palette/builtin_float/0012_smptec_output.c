/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: smptec output. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(0, 3, 1, 0);
}
