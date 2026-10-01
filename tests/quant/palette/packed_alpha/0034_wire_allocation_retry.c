/* SPDX-License-Identifier: MIT */
/* Verify packed alpha failure before output and reuse after recovery. */
int test_packed_wire(int space, int mode);

int
test_packed_0034(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire(0, 15);
}
