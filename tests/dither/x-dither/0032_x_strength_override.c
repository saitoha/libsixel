/* SPDX-License-Identifier: MIT */
/* Observe x strength override at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0075(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(1, 11, 0);
}
