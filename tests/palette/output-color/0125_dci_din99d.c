/* SPDX-License-Identifier: MIT */
/* Verify named destination dci din99d. */
int test_named_output(int, int, int, int);
int test_named_definition(int, int);

int
test_named_output_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_output(1, 4, 0, 0);
}
