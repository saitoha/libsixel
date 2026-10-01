/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: layout argb. */
int test_packed_unit(int first, int second);

int
test_packed_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(1, 0);
}
