/* SPDX-License-Identifier: MIT */
/* Ignored packed alpha retains an independent side mask. */
int test_highcolor_packed(int);

int
test_highcolor_0074(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(74);
}
