/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: p3 snap recovery. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0025(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(0, 1, 0, 11);
}
