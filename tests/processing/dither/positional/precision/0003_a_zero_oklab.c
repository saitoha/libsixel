/* SPDX-License-Identifier: MIT */
/* Verify a zero oklab at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(0, 0, 2);
}
