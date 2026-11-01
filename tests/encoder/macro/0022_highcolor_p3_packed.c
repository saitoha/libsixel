/* SPDX-License-Identifier: MIT */
/* Verify highcolor p3 packed. */
int test_highcolor_macro(int mode, char const *directory);

int
test_macro_0022(int argc, char **argv)
{
    return test_highcolor_macro(5, argc > 1 ? argv[1] : 0);
}
