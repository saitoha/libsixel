/* SPDX-License-Identifier: MIT */
/* Byte RGB retains the separated alpha-zero mask. */
int test_highcolor_mask(int);

int
test_highcolor_0050(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(50);
}
