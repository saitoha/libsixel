/* SPDX-License-Identifier: MIT */
/* Verify blue bias lab at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0029(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(2, 1, 3);
}
