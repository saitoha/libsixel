/* SPDX-License-Identifier: MIT */
/* Pixel allocation failure preserves packed input for retry. */
int test_highcolor_packed(int);

int
test_highcolor_0080(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(80);
}
