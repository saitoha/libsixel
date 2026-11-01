/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: alpha disabled. */
int test_packed_unit(int first, int second);

int
test_packed_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(0, 2);
}
