/* SPDX-License-Identifier: MIT */
/* Verify mono environment. */
int test_delta_mono_options(int);

int
test_delta_native_0203(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_mono_options(2);
}
