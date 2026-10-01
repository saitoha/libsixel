/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: wire alpha disabled. */
int test_packed_wire(int first, int second);

int
test_packed_0031(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire(0, 12);
}
