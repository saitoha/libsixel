/* SPDX-License-Identifier: MIT */
/* fs CPU RGB preserves legacy retained pixels and validity. */
int indexed_equivalence_test(char const *, int, int, int);

int
test_indexed_equivalence_31(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_equivalence_test("fs", 0, 0, 1);
}
