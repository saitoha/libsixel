/* SPDX-License-Identifier: MIT */
/* Observe a auto float at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0065(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 8, 0);
}
