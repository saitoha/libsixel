/* SPDX-License-Identifier: MIT */
/* Verify highcolor p3 replay. */
int test_highcolor_macro(int mode, char const *directory);

int
test_macro_0025(int argc, char **argv)
{
    return test_highcolor_macro(8, argc > 1 ? argv[1] : 0);
}
