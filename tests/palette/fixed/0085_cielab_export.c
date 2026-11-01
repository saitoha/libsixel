/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: cielab export. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0027(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(3, 0, 1, 13);
}
