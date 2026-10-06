/* SPDX-License-Identifier: MIT */
/* Three-row final band completes pending colors. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0038(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(38, 64, 9);
}
