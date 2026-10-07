/* SPDX-License-Identifier: MIT */
/* Packed alpha normalization under P3 PNG output intent. */
int test_highcolor_packed(int);

int
test_highcolor_0083(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(83);
}
