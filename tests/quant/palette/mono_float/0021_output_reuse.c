/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: output reuse. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(0, 0, 0, 7);
}
