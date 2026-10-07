/* SPDX-License-Identifier: MIT */
/* Verify highcolor p3 invoke. */
int test_highcolor_macro(int mode, char const *directory);

int
test_macro_0018(int argc, char **argv)
{
    return test_highcolor_macro(1, argc > 1 ? argv[1] : 0);
}
