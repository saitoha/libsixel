/* SPDX-License-Identifier: MIT */
/* A four-thread body budget preserves pending final-band pixels. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0046(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(46, 32, 18);
}
