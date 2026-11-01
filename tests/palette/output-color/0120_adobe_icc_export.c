/* SPDX-License-Identifier: MIT */
/* Verify named destination adobe icc export. */
int test_named_output(int, int, int, int);
int test_named_definition(int, int);

int
test_named_output_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_definition(0, 1);
}
