/* SPDX-License-Identifier: MIT */
/* Verify named destination dci highcolor. */
int test_named_output(int, int, int, int);
int test_named_definition(int, int);

int
test_named_output_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_output(1, 1, 0, 1);
}
