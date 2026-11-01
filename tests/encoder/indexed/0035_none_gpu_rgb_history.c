/* SPDX-License-Identifier: MIT */
/* none GPU RGB preserves legacy retained pixels and validity. */
int indexed_equivalence_test(char const *, int, int, int);

int
test_indexed_equivalence_35(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_equivalence_test("none", 1, 0, 1);
}
