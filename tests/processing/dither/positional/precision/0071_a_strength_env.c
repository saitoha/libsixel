/* SPDX-License-Identifier: MIT */
/* Observe a strength env at the consuming lookup boundary. */
int positional_query_test(int, int, int);

int
test_positional_0071(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 10, 0);
}
