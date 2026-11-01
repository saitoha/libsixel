/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: wire cielab. */
int test_packed_wire(int first, int second);

int
test_packed_0018(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire(3, 0);
}
