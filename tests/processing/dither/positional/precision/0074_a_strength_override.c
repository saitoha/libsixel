/* SPDX-License-Identifier: MIT */
/* Observe a strength override at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0074(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 11, 0);
}
