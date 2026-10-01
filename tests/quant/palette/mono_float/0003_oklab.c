/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: oklab. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(2, 0, 0, 0);
}
