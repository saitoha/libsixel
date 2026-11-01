/* SPDX-License-Identifier: MIT */
/* Verify a reprepare at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 5, 0);
}
