/* SPDX-License-Identifier: MIT */
/* Float RGB retains the separated alpha-zero mask. */
int test_highcolor_mask(int);

int
test_highcolor_0051(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(51);
}
