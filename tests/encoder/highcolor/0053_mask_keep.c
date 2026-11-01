/* SPDX-License-Identifier: MIT */
/* A high-color source mask requests P2=1 for keep. */
int test_highcolor_mask(int);

int
test_highcolor_0053(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(53);
}
