/* SPDX-License-Identifier: MIT */
/* Four-row final band completes pending colors. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0039(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(39, 64, 10);
}
