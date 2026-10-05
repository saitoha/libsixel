/* SPDX-License-Identifier: MIT */
/* Native x delta: source contract. */
int test_delta_positional_query(int, int);

int
test_delta_native_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_positional_query(1, 0);
}
