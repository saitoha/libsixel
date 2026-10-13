/* SPDX-License-Identifier: MIT */
/* Verify named destination dci icc export. */
int test_named_output(int, int, int, int);
int test_named_definition(int, int);

int
test_named_output_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_definition(1, 1);
}
