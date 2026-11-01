/* SPDX-License-Identifier: MIT */
/* The 256th key is painted by a pending final pass. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0048(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(48, 256, 1);
}
