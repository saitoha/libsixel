/* SPDX-License-Identifier: MIT */
/* A multi-band top margin is emitted only once across short passes. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0045(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(45, 256, 2);
}
