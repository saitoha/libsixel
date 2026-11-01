/* SPDX-License-Identifier: MIT */
/* An unaligned transparent margin preserves final-band placement. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0044(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(44, 32, 18);
}
