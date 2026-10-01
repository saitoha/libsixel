/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: mask allocation retry. */
int test_packed_unit(int first, int second);

int
test_packed_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(0, 8);
}
