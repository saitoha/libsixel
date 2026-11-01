/* SPDX-License-Identifier: MIT */
/* none GPU RGB preserves legacy stream bytes. */
int indexed_equivalence_test(char const *, int, int, int);

int
test_indexed_equivalence_34(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_equivalence_test("none", 1, 0, 0);
}
