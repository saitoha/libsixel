/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: wire gamma. */
int test_packed_wire(int first, int second);

int
test_packed_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire(0, 0);
}
