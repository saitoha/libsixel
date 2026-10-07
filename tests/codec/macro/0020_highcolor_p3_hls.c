/* SPDX-License-Identifier: MIT */
/* Verify highcolor p3 hls. */
int test_highcolor_macro(int mode, char const *directory);

int
test_macro_0020(int argc, char **argv)
{
    return test_highcolor_macro(3, argc > 1 ? argv[1] : 0);
}
