/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: wire din99d. */
int test_packed_wire(int first, int second);

int
test_packed_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire(4, 0);
}
