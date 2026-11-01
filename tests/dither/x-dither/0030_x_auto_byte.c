/* SPDX-License-Identifier: MIT */
/* Observe x auto byte at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0069(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(1, 9, 0);
}
