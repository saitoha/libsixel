/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: pixel allocation retry. */
int test_packed_unit(int first, int second);

int
test_packed_0013(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(0, 7);
}
