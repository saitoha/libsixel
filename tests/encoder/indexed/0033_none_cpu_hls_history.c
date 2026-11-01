/* SPDX-License-Identifier: MIT */
/* none CPU HLS preserves legacy retained pixels and validity. */
int indexed_equivalence_test(char const *, int, int, int);

int
test_indexed_equivalence_33(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_equivalence_test("none", 0, 1, 1);
}
