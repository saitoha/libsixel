/* SPDX-License-Identifier: MIT */
/* Verify x zero cielab at the consuming boundary. */
int positional_query_test(int, int, int);

int
test_positional_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_query_test(1, 0, 3);
}
