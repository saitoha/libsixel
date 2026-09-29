/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: byte fastpath. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(4, 0);
}
