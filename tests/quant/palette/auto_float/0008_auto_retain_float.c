/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: retain float. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(3, 0);
}
