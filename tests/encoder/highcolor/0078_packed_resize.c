/* SPDX-License-Identifier: MIT */
/* Nearest resize preserves normalized colors and omissions. */
int test_highcolor_packed(int);

int
test_highcolor_0078(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(78);
}
