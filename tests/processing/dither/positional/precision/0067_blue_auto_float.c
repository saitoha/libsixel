/* SPDX-License-Identifier: MIT */
/* Observe blue auto float at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0067(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(2, 8, 0);
}
