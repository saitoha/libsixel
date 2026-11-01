/* SPDX-License-Identifier: MIT */
/* fs CPU RGB preserves legacy stream bytes. */
int indexed_equivalence_test(char const *, int, int, int);

int
test_indexed_equivalence_30(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_equivalence_test("fs", 0, 0, 0);
}
