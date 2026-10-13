/* SPDX-License-Identifier: MIT */
/* Verify named destination adobe oklab. */
int test_named_output(int, int, int, int);
int test_named_definition(int, int);

int
test_named_output_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_output(0, 2, 0, 0);
}
