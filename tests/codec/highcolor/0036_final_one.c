/* SPDX-License-Identifier: MIT */
/* One-row final band completes pending colors. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0036(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(36, 256, 7);
}
