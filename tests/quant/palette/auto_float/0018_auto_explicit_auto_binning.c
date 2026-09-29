/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: explicit auto binning. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0018(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(13, 0);
}
