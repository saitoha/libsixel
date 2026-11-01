/* SPDX-License-Identifier: MIT */
/* none CPU HLS preserves legacy stream bytes. */
int indexed_equivalence_test(char const *, int, int, int);

int
test_indexed_equivalence_32(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_equivalence_test("none", 0, 1, 0);
}
