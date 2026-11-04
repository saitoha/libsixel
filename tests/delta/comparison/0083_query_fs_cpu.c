/* SPDX-License-Identifier: MIT */
int delta_compare_encode_test(int, int, int, int, int, int);

int
test_delta_compare_0083(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return delta_compare_encode_test(0, 0, 1, 2, 0, 0);
}
