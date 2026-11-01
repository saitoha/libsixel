/* SPDX-License-Identifier: MIT */
/* Verify highcolor p3 c1. */
int test_highcolor_macro(int mode, char const *directory);

int
test_macro_0019(int argc, char **argv)
{
    return test_highcolor_macro(2, argc > 1 ? argv[1] : 0);
}
