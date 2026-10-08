/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: P3 snap. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(11, 0);
}
