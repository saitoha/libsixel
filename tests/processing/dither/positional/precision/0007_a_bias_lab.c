/* SPDX-License-Identifier: MIT */
/* Verify a bias lab at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 1, 3);
}
