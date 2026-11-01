/* SPDX-License-Identifier: MIT */
/* An all-zero mask does not omit visible pixels. */
int test_highcolor_mask(int);

int
test_highcolor_0063(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(63);
}
