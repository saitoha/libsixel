/* SPDX-License-Identifier: MIT */
/* Explicit float promotes binary packed alpha. */
int test_highcolor_packed(int);

int
test_highcolor_0073(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(73);
}
