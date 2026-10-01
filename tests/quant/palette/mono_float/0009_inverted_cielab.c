/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: inverted cielab. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(3, 0, 1, 0);
}
