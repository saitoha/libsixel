/* SPDX-License-Identifier: MIT */
/* Verify blue zero gamma at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(2, 0, 0);
}
