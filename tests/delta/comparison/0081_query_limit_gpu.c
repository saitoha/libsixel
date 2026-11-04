/* SPDX-License-Identifier: MIT */
int delta_compare_encode_test(int, int, int, int, int, int);

int
test_delta_compare_0081(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return delta_compare_encode_test(1, 0, 1, 0, 0, 1);
}
