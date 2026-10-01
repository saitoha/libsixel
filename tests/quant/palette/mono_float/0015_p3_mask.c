/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: p3 mask. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(3, 1, 0, 0);
}
