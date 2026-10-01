/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: layout rgba. */
int test_packed_unit(int first, int second);

int
test_packed_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(0, 0);
}
