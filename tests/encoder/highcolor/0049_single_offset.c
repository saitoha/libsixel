/* SPDX-License-Identifier: MIT */
/* A short offset image retains its row bit over repeated passes. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0049(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(49, 768, 1);
}
