/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: default oklab. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(0, 2);
}
