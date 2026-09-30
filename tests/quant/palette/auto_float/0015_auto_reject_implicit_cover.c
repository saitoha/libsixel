/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: reject implicit cover. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(10, 0);
}
