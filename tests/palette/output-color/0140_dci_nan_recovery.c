/* SPDX-License-Identifier: MIT */
/* Verify named destination nan recovery. */
int test_named_output(int, int, int, int);

int
test_named_output_0034(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_named_output(1, 1, 0, 7);
}
