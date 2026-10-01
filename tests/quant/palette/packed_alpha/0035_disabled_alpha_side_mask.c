/* SPDX-License-Identifier: MIT */
/* Preserve a side mask independently of disabled packed alpha metadata. */
int test_packed_unit(int format, int mode);

int
test_packed_0035(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(0, 9);
}
