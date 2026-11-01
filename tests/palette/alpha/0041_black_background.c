/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: black background. */
int test_packed_unit(int first, int second);

int
test_packed_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(0, 1);
}
