/* SPDX-License-Identifier: MIT */
/* Verify named destination adobe medoids. */
int test_named_output(int, int, int, int);
int test_named_definition(int, int);

int
test_named_output_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_output(0, 1, 2, 0);
}
