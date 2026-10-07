/* SPDX-License-Identifier: MIT */
/* BGRA P3 output retains values through linear work. */
int test_highcolor_packed(int);

int
test_highcolor_0068(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(68);
}
