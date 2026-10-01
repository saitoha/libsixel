/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: auto cielab. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0029(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(3, 0, 0, 14);
}
