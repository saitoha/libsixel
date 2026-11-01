/* SPDX-License-Identifier: MIT */
/* Full final band completes every pending color. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0035(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(35, 32, 18);
}
