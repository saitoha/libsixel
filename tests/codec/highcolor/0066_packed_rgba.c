/* SPDX-License-Identifier: MIT */
/* RGBA fractional composition survives a byte request. */
int test_highcolor_packed(int);

int
test_highcolor_0066(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(66);
}
