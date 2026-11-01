/* SPDX-License-Identifier: MIT */
/* Two-row final band completes pending colors. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0037(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(37, 64, 8);
}
