/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: layout abgr. */
int test_packed_unit(int first, int second);

int
test_packed_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(3, 0);
}
