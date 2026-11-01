/* SPDX-License-Identifier: MIT */
/* Verify a zero linear at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 0, 1);
}
