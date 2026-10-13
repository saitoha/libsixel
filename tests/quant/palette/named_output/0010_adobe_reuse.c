/* SPDX-License-Identifier: MIT */
/* Verify named destination adobe reuse. */
int test_named_output(int, int, int, int);
int test_named_definition(int, int);

int
test_named_output_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_output(0, 1, 0, 2);
}
