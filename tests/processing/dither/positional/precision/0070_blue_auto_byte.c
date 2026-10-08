/* SPDX-License-Identifier: MIT */
/* Observe blue auto byte at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0070(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(2, 9, 0);
}
