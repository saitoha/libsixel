/* SPDX-License-Identifier: MIT */
/* Verify highcolor output reuse. */
int test_highcolor_macro(int mode, char const *directory);

int
test_macro_0024(int argc, char **argv)
{
    return test_highcolor_macro(7, argc > 1 ? argv[1] : 0);
}
