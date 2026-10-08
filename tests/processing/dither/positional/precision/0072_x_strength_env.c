/* SPDX-License-Identifier: MIT */
/* Observe x strength env at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0072(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(1, 10, 0);
}
