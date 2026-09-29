/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: default p3. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(2, 0);
}
