/* SPDX-License-Identifier: MIT */
/* Alpha-gray honors the background through DIN99d and P3. */
int test_highcolor_packed(int);

int
test_highcolor_0071(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(71);
}
