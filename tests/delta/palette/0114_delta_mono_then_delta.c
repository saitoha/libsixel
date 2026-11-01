/* SPDX-License-Identifier: MIT */
/* Verify mono then delta. */
int test_delta_mono_options(int);

int
test_delta_native_0201(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_mono_options(0);
}
