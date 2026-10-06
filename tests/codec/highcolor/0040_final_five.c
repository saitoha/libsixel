/* SPDX-License-Identifier: MIT */
/* Five-row final band completes pending colors. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0040(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(40, 64, 11);
}
