/* SPDX-License-Identifier: MIT */
/* Verify blue gradient endpoints at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0034(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(2, 6, 0);
}
