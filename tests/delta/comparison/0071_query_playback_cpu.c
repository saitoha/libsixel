/* SPDX-License-Identifier: MIT */
int delta_compare_encode_test(int, int, int, int, int, int);

int
test_delta_compare_0071(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return delta_compare_encode_test(0, 1, 1, 0, 0, 0);
}
