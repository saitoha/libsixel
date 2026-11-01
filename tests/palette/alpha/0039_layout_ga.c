/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: layout ga. */
int test_packed_unit(int first, int second);

int
test_packed_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(4, 0);
}
