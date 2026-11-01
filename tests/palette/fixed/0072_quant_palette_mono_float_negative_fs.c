/* SPDX-License-Identifier: MIT */
/* Verify native monochrome: negative fs. */
int test_mono_native(int space, int output, int invert, int mode);

int
test_mono_native_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_native(0, 0, 0, 3);
}
