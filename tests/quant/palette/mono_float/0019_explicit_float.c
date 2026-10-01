/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: explicit float. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(0, 0, 0, 5);
}
