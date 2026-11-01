/* SPDX-License-Identifier: MIT */
/* Verify a zero din99d at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 0, 4);
}
