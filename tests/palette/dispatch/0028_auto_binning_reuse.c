/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: binning reuse. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(9, 0);
}
