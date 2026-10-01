/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: wire background. */
int test_packed_wire(int first, int second);

int
test_packed_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire(0, 2);
}
