/* SPDX-License-Identifier: MIT */
int delta_compare_encode_test(int, int, int, int, int, int);

int
test_delta_compare_0078(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return delta_compare_encode_test(1, 1, 1, 0, 0, 0);
}
