/* SPDX-License-Identifier: MIT */
/* ABGR P3 output retains values through Oklab work. */
int test_highcolor_packed(int);

int
test_highcolor_0069(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(69);
}
