/* SPDX-License-Identifier: MIT */
/* A single row supports more than two palette passes. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0041(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(41, 768, 1);
}
