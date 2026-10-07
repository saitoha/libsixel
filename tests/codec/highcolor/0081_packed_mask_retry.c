/* SPDX-License-Identifier: MIT */
/* Mask allocation failure preserves packed input for retry. */
int test_highcolor_packed(int);

int
test_highcolor_0081(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(81);
}
