/* SPDX-License-Identifier: MIT */
/* none CPU RGB preserves legacy stream bytes. */
int indexed_equivalence_test(char const *, int, int, int);

int
test_indexed_equivalence_26(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_equivalence_test("none", 0, 0, 0);
}
