/* SPDX-License-Identifier: MIT */
/* Crop retains the normalized fractional colors and mask. */
int test_highcolor_packed(int);

int
test_highcolor_0077(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(77);
}
