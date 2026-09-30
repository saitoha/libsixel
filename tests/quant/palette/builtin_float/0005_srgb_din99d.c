/* SPDX-License-Identifier: MIT */
/* Verify built-in native palette: srgb din99d. */
int test_builtin_native(int space, int output, int table, int mode);

int
test_builtin_native_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_native(4, 0, 0, 0);
}
