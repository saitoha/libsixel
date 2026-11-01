/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: inversion reuse. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(3, 0, 0, 8);
}
