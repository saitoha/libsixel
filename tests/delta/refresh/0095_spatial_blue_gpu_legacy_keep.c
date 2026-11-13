/* SPDX-License-Identifier: MIT */
int delta_compare_pixel_test(int, int, int, int);

int
test_delta_refresh_0095(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return delta_compare_pixel_test(2, 0, 1, 7);
}
