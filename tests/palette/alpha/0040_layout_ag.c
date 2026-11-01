/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: layout ag. */
int test_packed_unit(int first, int second);

int
test_packed_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(5, 0);
}
