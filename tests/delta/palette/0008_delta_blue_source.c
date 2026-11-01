/* SPDX-License-Identifier: MIT */
/* Native blue delta: source contract. */
int test_delta_positional_query(int, int);

int
test_delta_native_0020(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_positional_query(2, 0);
}
