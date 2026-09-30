/* SPDX-License-Identifier: MIT */
/* Verify a bias gamma at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 1, 0);
}
