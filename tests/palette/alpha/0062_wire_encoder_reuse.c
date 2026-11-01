/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: wire encoder reuse. */
int test_packed_wire(int first, int second);

int
test_packed_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire(0, 9);
}
