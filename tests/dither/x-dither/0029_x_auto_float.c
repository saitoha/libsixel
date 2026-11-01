/* SPDX-License-Identifier: MIT */
/* Observe x auto float at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0066(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(1, 8, 0);
}
