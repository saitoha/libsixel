/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: wire oklab. */
int test_packed_wire(int first, int second);

int
test_packed_0017(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire(2, 0);
}
