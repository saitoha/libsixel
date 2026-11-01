/* SPDX-License-Identifier: MIT */
/* Verify named destination adobe cielab. */
int test_named_output(int, int, int, int);
int test_named_definition(int, int);

int
test_named_output_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_output(0, 3, 0, 0);
}
