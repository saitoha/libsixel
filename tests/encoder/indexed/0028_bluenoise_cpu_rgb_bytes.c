/* SPDX-License-Identifier: MIT */
/* bluenoise CPU RGB preserves legacy stream bytes. */
int indexed_equivalence_test(char const *, int, int, int);

int
test_indexed_equivalence_28(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_equivalence_test("bluenoise", 0, 0, 0);
}
