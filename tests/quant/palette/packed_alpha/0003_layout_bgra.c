/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: layout bgra. */
int test_packed_unit(int first, int second);

int
test_packed_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(2, 0);
}
